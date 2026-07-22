#!/usr/bin/env python3
"""
Command & Conquer Generals: Zero Hour - High APM Unified AI Learning & Control Agent
--------------------------------------------------------------------------------------
Single unified script that:
 1. Connects to the Generals C++ Game Engine via TCP socket (127.0.0.1:9999).
 2. Automatically detects match start, active play, match completion, and match restarts.
 3. Auto-reconnects seamlessly across multiple matches.
 4. Streams rich real-time telemetry (units, buildings, positions, money, power) at 20 Hz.
 5. Detects real actions from Players 2 through 8 via state deltas (building, unit training, movement).
 6. Balances action classes to prevent IDLE sample dominance.
 7. Continuously auto-trains a Deep Neural Network model (Behavioral Cloning) on Players 2 through 8.
 8. Remembers active match faction (USA, China, GLA) and maps structure/unit names dynamically.
 9. Computes dynamic building target coordinates relative to Player 1's Command Center / Dozer position.
 10. Executes Player 1 unit control at high APM with 3s build cooldowns to prevent build command thrashing.
"""

import os
import sys
import glob
import json
import gzip
import time
import math
import socket
import struct
import random
import threading
import logging
from typing import Dict, List, Any, Optional, Tuple

# Configure Logging
logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(name)s: %(message)s")
logger = logging.getLogger("GeneralsUnifiedAgent")

# PyTorch with pure NumPy Fallback
HAS_TORCH = False
try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    from torch.utils.data import Dataset, DataLoader
    HAS_TORCH = True
    logger.info("PyTorch loaded successfully for Neural Network training.")
except ImportError:
    logger.warning("PyTorch not found. Using NumPy for model training & inference.")
    import numpy as np


# Action Enumerations
ACTION_IDLE = 0
ACTION_BUILD_POWER_PLANT = 1
ACTION_BUILD_SUPPLY_CENTER = 2
ACTION_BUILD_BARRACKS = 3
ACTION_BUILD_WAR_FACTORY = 4
ACTION_BUILD_AIRFIELD = 5
ACTION_TRAIN_INFANTRY = 6
ACTION_TRAIN_TANK = 7
ACTION_TRAIN_AIRCRAFT = 8
ACTION_ATTACK_MOVE = 9

ACTION_NAMES = {
    ACTION_IDLE: "IDLE",
    ACTION_BUILD_POWER_PLANT: "BUILD_POWER_PLANT",
    ACTION_BUILD_SUPPLY_CENTER: "BUILD_SUPPLY_CENTER",
    ACTION_BUILD_BARRACKS: "BUILD_BARRACKS",
    ACTION_BUILD_WAR_FACTORY: "BUILD_WAR_FACTORY",
    ACTION_BUILD_AIRFIELD: "BUILD_AIRFIELD",
    ACTION_TRAIN_INFANTRY: "TRAIN_INFANTRY",
    ACTION_TRAIN_TANK: "TRAIN_TANK",
    ACTION_TRAIN_AIRCRAFT: "TRAIN_AIRCRAFT",
    ACTION_ATTACK_MOVE: "ATTACK_MOVE",
}


class FactionManager:
    """Detects active player faction (USA, China, GLA) and maps abstract macro actions to exact entities."""

    FACTION_MAPPINGS = {
        "USA": {
            "dozer": ["AmericaVehicleDozer", "Dozer"],
            "power_plant": "AmericaPowerPlant",
            "supply_center": "AmericaSupplyCenter",
            "barracks": "AmericaBarracks",
            "war_factory": "AmericaWarFactory",
            "airfield": "AmericaAirfield",
            "infantry": "AmericaRanger",
            "tank": "AmericaCrusader",
            "aircraft": "AmericaComanche",
        },
        "CHINA": {
            "dozer": ["ChinaVehicleDozer", "Dozer"],
            "power_plant": "ChinaPowerPlant",
            "supply_center": "ChinaSupplyCenter",
            "barracks": "ChinaBarracks",
            "war_factory": "ChinaWarFactory",
            "airfield": "ChinaAirfield",
            "infantry": "ChinaRedGuard",
            "tank": "ChinaBattleMaster",
            "aircraft": "ChinaMig",
        },
        "GLA": {
            "dozer": ["GLAVehicleWorker", "Worker"],
            "power_plant": "GLASupplyStash",
            "supply_center": "GLASupplyStash",
            "barracks": "GLABarracks",
            "war_factory": "GLAArmsDealer",
            "airfield": "GLABarracks",
            "infantry": "GLARebel",
            "tank": "GLAScorpion",
            "aircraft": "GLARebel",
        }
    }

    @staticmethod
    def detect_faction(player_data: Dict[str, Any], fallback: str = "USA") -> str:
        units = player_data.get("owned_units", [])
        buildings = player_data.get("owned_buildings", [])

        for u in units:
            u_type = u.get("type", "")
            if "GLA" in u_type or "Worker" in u_type or "Rebel" in u_type or "Scorpion" in u_type:
                return "GLA"
            elif "China" in u_type or "RedGuard" in u_type or "BattleMaster" in u_type:
                return "CHINA"
            elif "America" in u_type or "USA" in u_type or "Ranger" in u_type or "Crusader" in u_type:
                return "USA"

        for b in buildings:
            b_type = b.get("type", "")
            if "GLA" in b_type or "ArmsDealer" in b_type or "SupplyStash" in b_type:
                return "GLA"
            elif "China" in b_type or "Bunker" in b_type:
                return "CHINA"
            elif "America" in b_type or "Patriot" in b_type:
                return "USA"

        return fallback


class GameStateVectorizer:
    """Vectorizes telemetry data from Players into a 24-dimensional feature array."""
    STATE_DIM = 24

    @staticmethod
    def vectorize_player(player_data: Dict[str, Any], global_frame: int) -> List[float]:
        frame = float(global_frame) / 10000.0
        money = float(player_data.get("money", 0)) / 20000.0
        power = float(player_data.get("power", 0)) / 100.0
        max_power = float(player_data.get("max_power", 0)) / 100.0
        p_index = float(player_data.get("index", 0)) / 8.0

        owned_units = player_data.get("owned_units", [])
        owned_buildings = player_data.get("owned_buildings", [])

        dozers = sum(1 for u in owned_units if "Dozer" in u.get("type", "") or "Worker" in u.get("type", ""))
        infantry = sum(1 for u in owned_units if "Ranger" in u.get("type", "") or "Rebel" in u.get("type", "") or "RedGuard" in u.get("type", ""))
        tanks = sum(1 for u in owned_units if "Crusader" in u.get("type", "") or "BattleMaster" in u.get("type", "") or "Scorpion" in u.get("type", ""))
        aircraft = sum(1 for u in owned_units if "Comanche" in u.get("type", "") or "Raptor" in u.get("type", "") or "Mig" in u.get("type", ""))

        power_plants = sum(1 for b in owned_buildings if "PowerPlant" in b.get("type", "") or "Reactor" in b.get("type", ""))
        supply_centers = sum(1 for b in owned_buildings if "SupplyCenter" in b.get("type", "") or "SupplyStash" in b.get("type", ""))
        barracks = sum(1 for b in owned_buildings if "Barracks" in b.get("type", ""))
        factories = sum(1 for b in owned_buildings if "WarFactory" in b.get("type", "") or "ArmsDealer" in b.get("type", ""))
        airfields = sum(1 for b in owned_buildings if "Airfield" in b.get("type", ""))

        vec = [
            frame,
            money,
            power,
            max_power,
            p_index,
            1.0 if player_data.get("is_ai", True) else 0.0,
            float(dozers) / 10.0,
            float(power_plants) / 10.0,
            float(supply_centers) / 5.0,
            float(barracks) / 5.0,
            float(factories) / 5.0,
            float(airfields) / 5.0,
            float(infantry) / 50.0,
            float(tanks) / 50.0,
            float(aircraft) / 20.0,
        ]

        while len(vec) < GameStateVectorizer.STATE_DIM:
            vec.append(0.0)

        return vec


class ActionDetector:
    """Detects real player actions by tracking telemetry state deltas across frames."""

    def __init__(self):
        self.prev_player_states: Dict[int, Dict[str, Any]] = {}

    def detect_action(self, player_data: Dict[str, Any], frame: int) -> int:
        p_idx = player_data.get("index", 0)
        curr_units = player_data.get("owned_units", [])
        curr_buildings = player_data.get("owned_buildings", [])

        if p_idx not in self.prev_player_states:
            self.prev_player_states[p_idx] = {
                "units": curr_units,
                "buildings": curr_buildings,
                "frame": frame
            }
            return ACTION_IDLE

        prev_state = self.prev_player_states[p_idx]
        prev_units = prev_state["units"]
        prev_buildings = prev_state["buildings"]

        action = ACTION_IDLE

        # 1. Detect Building Construction
        if len(curr_buildings) > len(prev_buildings):
            new_b = curr_buildings[-1].get("type", "")
            if "PowerPlant" in new_b or "Reactor" in new_b:
                action = ACTION_BUILD_POWER_PLANT
            elif "SupplyCenter" in new_b or "SupplyStash" in new_b:
                action = ACTION_BUILD_SUPPLY_CENTER
            elif "Barracks" in new_b:
                action = ACTION_BUILD_BARRACKS
            elif "WarFactory" in new_b or "ArmsDealer" in new_b:
                action = ACTION_BUILD_WAR_FACTORY
            elif "Airfield" in new_b:
                action = ACTION_BUILD_AIRFIELD

        # 2. Detect Unit Production
        elif len(curr_units) > len(prev_units):
            new_u = curr_units[-1].get("type", "")
            if "Ranger" in new_u or "Rebel" in new_u or "RedGuard" in new_u:
                action = ACTION_TRAIN_INFANTRY
            elif "Crusader" in new_u or "BattleMaster" in new_u or "Scorpion" in new_u:
                action = ACTION_TRAIN_TANK
            elif "Comanche" in new_u or "Raptor" in new_u or "Mig" in new_u:
                action = ACTION_TRAIN_AIRCRAFT

        # 3. Detect Unit Movement / Position Changes
        elif len(curr_units) > 0 and len(prev_units) > 0:
            u_curr = curr_units[0]
            u_prev = next((u for u in prev_units if u.get("id") == u_curr.get("id")), None)
            if u_prev:
                dx = u_curr.get("x", 0) - u_prev.get("x", 0)
                dy = u_curr.get("y", 0) - u_prev.get("y", 0)
                if (dx * dx + dy * dy) > 25.0:  # Moved > 5 units distance
                    action = ACTION_ATTACK_MOVE

        # Update previous state cache
        self.prev_player_states[p_idx] = {
            "units": curr_units,
            "buildings": curr_buildings,
            "frame": frame
        }

        return action


class EngineSocketClient:
    """TCP Socket Client listening for telemetry and sending commands to Generals Engine."""

    def __init__(self, host: str = "127.0.0.1", port: int = 9999):
        self.host = host
        self.port = port
        self.sock: Optional[socket.socket] = None
        self.is_connected = False
        self.running = False
        self.state_lock = threading.Lock()
        self.latest_telemetry: Dict[str, Any] = {"frame": 0, "players": []}
        self.last_recv_time = 0.0

    def connect(self, retries: int = 5, delay_sec: float = 1.0) -> bool:
        for attempt in range(1, retries + 1):
            try:
                logger.info(f"Connecting to Generals Engine at {self.host}:{self.port} (Attempt {attempt}/{retries})...")
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.sock.settimeout(5.0)
                self.sock.connect((self.host, self.port))
                self.is_connected = True
                self.running = True
                self.last_recv_time = time.time()
                logger.info("Successfully connected to Generals Engine IPC Socket!")
                threading.Thread(target=self._listen_loop, daemon=True).start()
                return True
            except (ConnectionRefusedError, socket.timeout, OSError) as e:
                logger.warning(f"Connection attempt {attempt} failed: {e}")
                if self.sock:
                    self.sock.close()
                    self.sock = None
                time.sleep(delay_sec)

        logger.error(f"Could not connect to Generals Engine on port {self.port}.")
        return False

    def disconnect(self):
        self.running = False
        self.is_connected = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
            self.sock = None
        logger.info("Disconnected from Generals Engine Socket.")

    def _listen_loop(self):
        buffer = bytearray()
        while self.running and self.sock:
            try:
                data = self.sock.recv(65536)
                if not data:
                    break
                buffer.extend(data)
                while len(buffer) >= 4:
                    msg_len = struct.unpack("!I", buffer[:4])[0]
                    if len(buffer) < 4 + msg_len:
                        break
                    payload = buffer[4 : 4 + msg_len]
                    del buffer[: 4 + msg_len]
                    try:
                        parsed = json.loads(payload.decode("utf-8"))
                        with self.state_lock:
                            self.latest_telemetry.update(parsed)
                            self.last_recv_time = time.time()
                    except json.JSONDecodeError:
                        pass
            except socket.timeout:
                continue
            except Exception:
                break
        self.is_connected = False

    def send_command(self, cmd_type: str, params: Dict[str, Any]) -> bool:
        if not self.is_connected or not self.sock:
            return False
        try:
            payload = json.dumps({"player": 1, "command": cmd_type, "params": params}).encode("utf-8")
            header = struct.pack("!I", len(payload))
            self.sock.sendall(header + payload)
            return True
        except Exception:
            self.is_connected = False
            return False


if HAS_TORCH:
    class PlayerPolicyNetwork(nn.Module):
        def __init__(self, input_dim: int = 24, num_actions: int = 10, hidden_dim: int = 128):
            super().__init__()
            self.network = nn.Sequential(
                nn.Linear(input_dim, hidden_dim),
                nn.BatchNorm1d(hidden_dim),
                nn.ReLU(),
                nn.Linear(hidden_dim, hidden_dim),
                nn.ReLU(),
                nn.Linear(hidden_dim, num_actions)
            )

        def forward(self, x: torch.Tensor) -> torch.Tensor:
            return self.network(x)


class NumPyPolicyNetwork:
    def __init__(self, input_dim: int = 24, hidden_dim: int = 64, num_actions: int = 10):
        self.input_dim = input_dim
        self.hidden_dim = hidden_dim
        self.num_actions = num_actions
        import numpy as np
        self.W1 = np.random.randn(input_dim, hidden_dim) * np.sqrt(2.0 / input_dim)
        self.b1 = np.zeros((1, hidden_dim))
        self.W2 = np.random.randn(hidden_dim, num_actions) * np.sqrt(2.0 / hidden_dim)
        self.b2 = np.zeros((1, num_actions))

    def relu(self, x):
        import numpy as np
        return np.maximum(0, x)

    def softmax(self, x):
        import numpy as np
        exp_x = np.exp(x - np.max(x, axis=1, keepdims=True))
        return exp_x / np.sum(exp_x, axis=1, keepdims=True)

    def forward(self, X):
        import numpy as np
        self.z1 = np.dot(X, self.W1) + self.b1
        self.a1 = self.relu(self.z1)
        self.z2 = np.dot(self.a1, self.W2) + self.b2
        self.probs = self.softmax(self.z2)
        return self.probs

    def train_step(self, X, y_labels, lr: float = 0.01) -> float:
        import numpy as np
        m = X.shape[0]
        probs = self.forward(X)

        one_hot = np.zeros((m, self.num_actions))
        one_hot[np.arange(m), y_labels] = 1.0

        loss = -np.sum(one_hot * np.log(probs + 1e-12)) / m

        dz2 = (probs - one_hot) / m
        dW2 = np.dot(self.a1.T, dz2)
        db2 = np.sum(dz2, axis=0, keepdims=True)

        da1 = np.dot(dz2, self.W2.T)
        dz1 = da1 * (self.z1 > 0)
        dW1 = np.dot(X.T, dz1)
        db1 = np.sum(dz1, axis=0, keepdims=True)

        self.W1 -= lr * dW1
        self.b1 -= lr * db1
        self.W2 -= lr * dW2
        self.b2 -= lr * db2

        return float(loss)

    def save(self, filepath: str):
        import numpy as np
        np.savez(filepath, W1=self.W1, b1=self.b1, W2=self.W2, b2=self.b2)


class UnifiedGeneralsAI:
    """High APM Unified AI Trainer & Controller (Trains on Players 2-8, Controls Player 1)."""

    def __init__(self, client: EngineSocketClient, data_dir: str = "logs/dataset", model_dir: str = "models"):
        self.client = client
        self.data_dir = data_dir
        self.model_dir = model_dir
        self.running = False
        self.sample_count = 0
        self.total_match_samples = 0
        self.last_processed_frame = -1
        self.training_buffer: List[Tuple[List[float], int]] = []
        self.completed_matches = 0
        self.is_match_active = False
        self.last_action_time = 0.0
        self.last_build_time = 0.0
        self.action_cooldown_sec = 0.05  # Severe High APM Execution (~1200 APM / 20 Hz)
        self.build_cooldown_sec = 3.0    # 3s cooldown between structure construction commands

        self.last_detected_faction = "USA"
        self.action_detector = ActionDetector()

        os.makedirs(self.data_dir, exist_ok=True)
        os.makedirs(self.model_dir, exist_ok=True)

        self.log_file = None
        self.log_path = None
        self._init_new_match_log()

        self.model_path_pth = os.path.join(self.model_dir, "unified_player1_policy.pth")
        self.model_path_npz = os.path.join(self.model_dir, "unified_player1_policy.npz")

        if HAS_TORCH:
            self.model = PlayerPolicyNetwork(input_dim=24, num_actions=10)
            self.optimizer = optim.Adam(self.model.parameters(), lr=0.001)
            self.criterion = nn.CrossEntropyLoss()
        else:
            self.model = NumPyPolicyNetwork(input_dim=24, hidden_dim=64, num_actions=10)

        self.has_trained_model = self._load_model_weights()
        if self.has_trained_model:
            logger.info(">>> Pre-trained policy model loaded! Player 1 unit control is ACTIVE from start. <<<")
        else:
            logger.info(">>> No existing policy model found. Extracting & Auto-Training on Match 1. Player 1 control will ACTIVATE after 1 match. <<<")

    def _init_new_match_log(self):
        if self.log_file:
            try:
                self.log_file.close()
            except Exception:
                pass
        timestamp = time.strftime("%Y%m%d_%H%M%S")
        self.log_path = os.path.join(self.data_dir, f"ai_match_{timestamp}.jsonl.gz")
        self.log_file = gzip.open(self.log_path, "wt", encoding="utf-8")
        logger.info(f"[Dataset Logger] Initialized match log file: {self.log_path}")

    def _load_model_weights(self) -> bool:
        if HAS_TORCH and os.path.exists(self.model_path_pth):
            try:
                self.model.load_state_dict(torch.load(self.model_path_pth))
                self.model.eval()
                logger.info(f"Loaded PyTorch policy weights from '{self.model_path_pth}'.")
                return True
            except Exception as e:
                logger.error(f"Failed to load PyTorch model weights: {e}")
        elif not HAS_TORCH and os.path.exists(self.model_path_npz):
            try:
                import numpy as np
                data = np.load(self.model_path_npz)
                self.model.W1 = data["W1"]
                self.model.b1 = data["b1"]
                self.model.W2 = data["W2"]
                self.model.b2 = data["b2"]
                logger.info(f"Loaded NumPy policy weights from '{self.model_path_npz}'.")
                return True
            except Exception as e:
                logger.error(f"Failed to load NumPy model weights: {e}")
        return False

    def process_telemetry(self, telemetry: Dict[str, Any]):
        """Process real-time game state telemetry, train on Players 2-8, and execute Player 1 control."""
        frame = telemetry.get("frame", 0)
        players = telemetry.get("players", [])

        if frame == 0 or not players:
            return

        # Detect Match Restart / Reset
        if self.last_processed_frame > 0 and frame < self.last_processed_frame:
            logger.info(f"[Match Lifecycle] New match detected! Frame reset from {self.last_processed_frame} to {frame}.")
            self._handle_match_end()
            self._init_new_match_log()

        self.last_processed_frame = frame
        self.is_match_active = True

        # Extract telemetry and train ONLY on Players 2 through 8 (index >= 1)
        ai_players = [p for p in players if p.get("index", 0) >= 1]
        for p in ai_players:
            state_vec = GameStateVectorizer.vectorize_player(p, frame)
            detected_action = self.action_detector.detect_action(p, frame)

            # Class Balancing: Downsample IDLE frames to avoid 95% IDLE dominance
            if detected_action == ACTION_IDLE:
                if random.random() > 0.15:
                    continue

            record = {
                "timestamp": time.time(),
                "frame": frame,
                "player_index": p.get("index"),
                "state_vector": state_vec,
                "action_id": detected_action,
                "action_name": ACTION_NAMES.get(detected_action, "IDLE")
            }
            if self.log_file:
                self.log_file.write(json.dumps(record) + "\n")

            self.training_buffer.append((state_vec, detected_action))
            self.sample_count += 1
            self.total_match_samples += 1

            if len(self.training_buffer) > 10000:
                self.training_buffer = self.training_buffer[-10000:]

        if self.total_match_samples > 0 and self.total_match_samples % 50 == 0:
            if self.log_file:
                self.log_file.flush()
            logger.info(f"[Telemetry Extractor] Extracted {self.total_match_samples} samples from Players 2-8 (Total overall: {self.sample_count}, Frame {frame}).")

        # Continuous online mini-batch update during active match
        if len(self.training_buffer) >= 20 and self.sample_count % 10 == 0:
            self.run_online_training_step()

        # Control Player 1 after 1 match or if pre-trained model is loaded
        if (self.has_trained_model or self.completed_matches >= 1) and (time.time() - self.last_action_time >= self.action_cooldown_sec):
            self.execute_player1_control(telemetry)

    def run_online_training_step(self):
        if len(self.training_buffer) < 10:
            return

        batch = self.training_buffer[-64:]
        if HAS_TORCH:
            self.model.train()
            states = torch.tensor([b[0] for b in batch], dtype=torch.float32)
            actions = torch.tensor([b[1] for b in batch], dtype=torch.long)

            self.optimizer.zero_grad()
            outputs = self.model(states)
            loss = self.criterion(outputs, actions)
            loss.backward()
            self.optimizer.step()

            logger.info(f"[Online Trainer] Loss on Players 2-8: {loss.item():.4f} (Buffer: {len(self.training_buffer)} samples)")
        else:
            import numpy as np
            X_batch = np.array([b[0] for b in batch], dtype=np.float32)
            y_batch = np.array([b[1] for b in batch], dtype=np.int32)
            loss = self.model.train_step(X_batch, y_batch, lr=0.01)
            logger.info(f"[Online NumPy Trainer] Loss on Players 2-8: {loss:.4f} (Buffer: {len(self.training_buffer)} samples)")

    def run_full_training_epochs(self, epochs: int = 5, batch_size: int = 32):
        if len(self.training_buffer) < 10:
            logger.warning("[Auto-Trainer] Training buffer has too few samples for epoch pass.")
            return

        logger.info(f"=== [Auto-Trainer] Running {epochs} full training epochs on {len(self.training_buffer)} samples from Players 2-8... ===")
        if HAS_TORCH:
            self.model.train()
            dataset_states = torch.tensor([b[0] for b in self.training_buffer], dtype=torch.float32)
            dataset_actions = torch.tensor([b[1] for b in self.training_buffer], dtype=torch.long)
            dataset = torch.utils.data.TensorDataset(dataset_states, dataset_actions)
            dataloader = DataLoader(dataset, batch_size=batch_size, shuffle=True)

            for epoch in range(1, epochs + 1):
                total_loss = 0.0
                for states, labels in dataloader:
                    self.optimizer.zero_grad()
                    outputs = self.model(states)
                    loss = self.criterion(outputs, labels)
                    loss.backward()
                    self.optimizer.step()
                    total_loss += loss.item() * states.size(0)

                avg_loss = total_loss / max(1, len(dataset))
                logger.info(f"  [PyTorch Epoch {epoch}/{epochs}] Loss: {avg_loss:.4f}")

            torch.save(self.model.state_dict(), self.model_path_pth)
            logger.info(f"[Auto-Trainer] Model saved successfully to {self.model_path_pth}")
        else:
            import numpy as np
            X = np.array([b[0] for b in self.training_buffer], dtype=np.float32)
            y = np.array([b[1] for b in self.training_buffer], dtype=np.int32)
            num_samples = X.shape[0]

            for epoch in range(1, epochs + 1):
                indices = np.arange(num_samples)
                np.random.shuffle(indices)
                X_shuffled = X[indices]
                y_shuffled = y[indices]

                epoch_loss = 0.0
                num_batches = math.ceil(num_samples / batch_size)
                for b in range(num_batches):
                    X_batch = X_shuffled[b * batch_size : (b + 1) * batch_size]
                    y_batch = y_shuffled[b * batch_size : (b + 1) * batch_size]
                    loss = self.model.train_step(X_batch, y_batch, lr=0.01)
                    epoch_loss += loss

                avg_loss = epoch_loss / max(1, num_batches)
                logger.info(f"  [NumPy Epoch {epoch}/{epochs}] Loss: {avg_loss:.4f}")

            self.model.save(self.model_path_npz)
            logger.info(f"[Auto-Trainer] NumPy Model saved successfully to {self.model_path_npz}")

    def _handle_match_end(self):
        self.completed_matches += 1
        logger.info(f"==========================================================================")
        logger.info(f"=== [Match Lifecycle] Match #{self.completed_matches} Finished! Samples logged: {self.total_match_samples} ===")
        logger.info(f"==========================================================================")

        if self.log_file:
            try:
                self.log_file.flush()
            except Exception:
                pass

        self.run_full_training_epochs(epochs=5)
        self.has_trained_model = True
        logger.info(f"=== [Player 1 Controller] Player 1 Unit Control is now ACTIVE! ===")

        self.total_match_samples = 0
        self.last_processed_frame = -1
        self.is_match_active = False

    def predict_action(self, state_vector: List[float]) -> int:
        if HAS_TORCH and self.model is not None:
            self.model.eval()
            with torch.no_grad():
                tensor_in = torch.tensor([state_vector], dtype=torch.float32)
                logits = self.model(tensor_in)
                action_id = int(logits.argmax(dim=1).item())
                return action_id
        elif not HAS_TORCH and self.model is not None:
            import numpy as np
            arr_in = np.array([state_vector], dtype=np.float32)
            probs = self.model.forward(arr_in)
            action_id = int(np.argmax(probs, axis=1)[0])
            return action_id
        return ACTION_IDLE

    def execute_player1_control(self, telemetry: Dict[str, Any]):
        """Evaluates trained model and sends Player 1 commands over IPC socket at high APM."""
        frame = telemetry.get("frame", 0)
        players = telemetry.get("players", [])

        p1_data = next((p for p in players if p.get("index") == 0), {"index": 0, "money": 10000, "is_ai": False})
        state_vec = GameStateVectorizer.vectorize_player(p1_data, frame)

        predicted_action = self.predict_action(state_vec)
        now = time.time()
        self.last_action_time = now

        owned_units = p1_data.get("owned_units", [])
        owned_buildings = p1_data.get("owned_buildings", [])

        # Detect active Player 1 Faction (USA, China, GLA) and cache detected faction
        detected_f = FactionManager.detect_faction(p1_data, fallback=self.last_detected_faction)
        if detected_f in FactionManager.FACTION_MAPPINGS:
            self.last_detected_faction = detected_f

        faction = self.last_detected_faction
        f_map = FactionManager.FACTION_MAPPINGS[faction]

        # Find Dozers, Barracks, Factories, Airfields, and Combat Units for active faction
        dozers = [u for u in owned_units if any(d_type in u.get("type", "") for d_type in f_map["dozer"])]
        barracks = [b for b in owned_buildings if f_map["barracks"] in b.get("type", "")]
        factories = [b for b in owned_buildings if f_map["war_factory"] in b.get("type", "")]
        airfields = [b for b in owned_buildings if f_map["airfield"] in b.get("type", "")]
        combat_units = [u["id"] for u in owned_units if not any(d_type in u.get("type", "") for d_type in f_map["dozer"])]

        # Find reference position for base building (Dozer/Worker position or Command Center position)
        ref_x, ref_y = 200.0, 200.0
        if dozers:
            ref_x, ref_y = dozers[0].get("x", 200.0), dozers[0].get("y", 200.0)
        elif owned_buildings:
            ref_x, ref_y = owned_buildings[0].get("x", 200.0), owned_buildings[0].get("y", 200.0)

        # Check existing building types for tech tree prerequisites
        has_power = any(f_map["power_plant"] in b.get("type", "") for b in owned_buildings) or (faction == "GLA")
        has_supply = any(f_map["supply_center"] in b.get("type", "") for b in owned_buildings)
        has_barracks = any(f_map["barracks"] in b.get("type", "") for b in owned_buildings)
        has_war_factory = any(f_map["war_factory"] in b.get("type", "") for b in owned_buildings)

        action_to_execute = predicted_action

        # ENFORCE EARLY GAME OPENING BUILD ORDER PRIORITIES:
        # If no base infrastructure exists yet, prioritize building base structures
        if len(owned_buildings) <= 1 or not has_war_factory:
            if not has_power:
                action_to_execute = ACTION_BUILD_POWER_PLANT
            elif not has_supply and not has_barracks:
                action_to_execute = ACTION_BUILD_SUPPLY_CENTER
            elif not has_war_factory:
                action_to_execute = ACTION_BUILD_WAR_FACTORY
            elif not barracks:
                action_to_execute = ACTION_BUILD_BARRACKS

        # Tech Tree Prerequisite Validation:
        if action_to_execute == ACTION_BUILD_WAR_FACTORY and not has_power:
            action_to_execute = ACTION_BUILD_POWER_PLANT

        elif action_to_execute == ACTION_BUILD_WAR_FACTORY and not (has_supply or has_barracks):
            action_to_execute = ACTION_BUILD_BARRACKS

        elif action_to_execute == ACTION_BUILD_AIRFIELD and (not has_war_factory or faction == "GLA"):
            action_to_execute = ACTION_BUILD_WAR_FACTORY if faction != "GLA" else ACTION_BUILD_BARRACKS

        # Structure Build Execution (with 3s build cooldown)
        is_build_action = action_to_execute in (
            ACTION_BUILD_POWER_PLANT, ACTION_BUILD_SUPPLY_CENTER,
            ACTION_BUILD_BARRACKS, ACTION_BUILD_WAR_FACTORY, ACTION_BUILD_AIRFIELD
        )

        dozer_id = dozers[0]["id"] if dozers else 0

        if is_build_action and (now - self.last_build_time >= self.build_cooldown_sec):
            self.last_build_time = now

            if action_to_execute == ACTION_BUILD_POWER_PLANT:
                target_x, target_y = ref_x - 50.0, ref_y + 40.0
                self.client.send_command("BUILD_STRUCTURE", {"dozer_id": dozer_id, "structure_type": f_map["power_plant"], "x": target_x, "y": target_y})
                logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] BUILD_STRUCTURE -> {f_map['power_plant']} at ({target_x:.1f}, {target_y:.1f})")

            elif action_to_execute == ACTION_BUILD_SUPPLY_CENTER:
                target_x, target_y = ref_x + 60.0, ref_y - 30.0
                self.client.send_command("BUILD_STRUCTURE", {"dozer_id": dozer_id, "structure_type": f_map["supply_center"], "x": target_x, "y": target_y})
                logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] BUILD_STRUCTURE -> {f_map['supply_center']} at ({target_x:.1f}, {target_y:.1f})")

            elif action_to_execute == ACTION_BUILD_BARRACKS:
                target_x, target_y = ref_x + 50.0, ref_y + 50.0
                self.client.send_command("BUILD_STRUCTURE", {"dozer_id": dozer_id, "structure_type": f_map["barracks"], "x": target_x, "y": target_y})
                logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] BUILD_STRUCTURE -> {f_map['barracks']} at ({target_x:.1f}, {target_y:.1f})")

            elif action_to_execute == ACTION_BUILD_WAR_FACTORY:
                target_x, target_y = ref_x - 60.0, ref_y - 50.0
                self.client.send_command("BUILD_STRUCTURE", {"dozer_id": dozer_id, "structure_type": f_map["war_factory"], "x": target_x, "y": target_y})
                logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] BUILD_STRUCTURE -> {f_map['war_factory']} at ({target_x:.1f}, {target_y:.1f})")

            elif action_to_execute == ACTION_BUILD_AIRFIELD and faction != "GLA":
                target_x, target_y = ref_x + 80.0, ref_y + 80.0
                self.client.send_command("BUILD_STRUCTURE", {"dozer_id": dozer_id, "structure_type": f_map["airfield"], "x": target_x, "y": target_y})
                logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] BUILD_STRUCTURE -> {f_map['airfield']} at ({target_x:.1f}, {target_y:.1f})")

        elif action_to_execute == ACTION_TRAIN_INFANTRY and barracks:
            self.client.send_command("PRODUCE_UNIT", {"factory_id": barracks[0]["id"], "unit_type": f_map["infantry"]})
            logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] PRODUCE_UNIT -> {f_map['infantry']}")

        elif action_to_execute == ACTION_TRAIN_TANK and factories:
            self.client.send_command("PRODUCE_UNIT", {"factory_id": factories[0]["id"], "unit_type": f_map["tank"]})
            logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] PRODUCE_UNIT -> {f_map['tank']}")

        elif action_to_execute == ACTION_TRAIN_AIRCRAFT and airfields:
            self.client.send_command("PRODUCE_UNIT", {"factory_id": airfields[0]["id"], "unit_type": f_map["aircraft"]})
            logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] PRODUCE_UNIT -> {f_map['aircraft']}")

        elif len(combat_units) > 0:
            target_x = 400.0 + random.uniform(-30.0, 30.0)
            target_y = 400.0 + random.uniform(-30.0, 30.0)
            self.client.send_command("ATTACK_MOVE", {"unit_ids": combat_units, "x": target_x, "y": target_y})
            logger.info(f"[Player 1 Bot (Faction {faction}, Frame {frame})] ATTACK_MOVE for {len(combat_units)} units to ({target_x:.1f}, {target_y:.1f})")

    def main_loop(self, poll_interval_sec: float = 0.05):
        self.running = True
        logger.info("==========================================================================")
        logger.info("Starting High APM Unified AI Agent Pipeline for Command & Conquer Generals...")
        logger.info(" - Telemetry Rate: 20 Hz (Severe High APM Mode)")
        logger.info(" - Dynamic Building Target Location Placement Enabled")
        logger.info(" - Dozer ID Tracking & 3s Build Cooldown Enabled")
        logger.info("==========================================================================")

        while self.running:
            try:
                if not self.client.is_connected:
                    if self.is_match_active:
                        logger.info("[Match Lifecycle] Game socket disconnected (Match finished).")
                        self._handle_match_end()
                    logger.info("Waiting to reconnect to Generals Engine at 127.0.0.1:9999...")
                    if self.client.connect(retries=1, delay_sec=1.0):
                        logger.info("Reconnected successfully to Generals Engine IPC Socket!")
                        self._init_new_match_log()
                    else:
                        time.sleep(poll_interval_sec)
                        continue

                now = time.time()
                time_since_last_recv = now - self.client.last_recv_time

                if time_since_last_recv > 3.0:
                    if self.is_match_active:
                        logger.info("[Match Lifecycle] Telemetry stream paused/ended (Match finished or returned to menu).")
                        self._handle_match_end()
                    time.sleep(poll_interval_sec)
                    continue

                with self.client.state_lock:
                    telemetry = dict(self.client.latest_telemetry)

                self.process_telemetry(telemetry)

            except Exception as e:
                logger.error(f"Error in AI pipeline iteration: {e}")

            time.sleep(poll_interval_sec)

        if self.log_file:
            try:
                self.log_file.close()
            except Exception:
                pass


def main():
    client = EngineSocketClient(host="127.0.0.1", port=9999)
    if client.connect(retries=10, delay_sec=2.0):
        agent = UnifiedGeneralsAI(client)
        try:
            agent.main_loop(poll_interval_sec=0.05)
        except KeyboardInterrupt:
            logger.info("Stopped by user (Ctrl+C).")
        finally:
            agent.running = False
            client.disconnect()
    else:
        logger.info("Generals game engine not connected on port 9999.")
        logger.info("Start generalszh.exe first, then run this script:")
        logger.info("  python scripts/ai/unified_player1_agent.py")


if __name__ == "__main__":
    main()
