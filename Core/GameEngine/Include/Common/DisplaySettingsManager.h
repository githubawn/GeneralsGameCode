/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#pragma once

#include "Lib/BaseType.h"
#include "Common/SubsystemInterface.h"
#include "Common/STLTypedefs.h"

/**
 * Manages the display state for the entire engine.
 * Acts as the single source of truth for resolution and windowing settings.
 * Centralizing this logic fixes issues where GlobalData, OptionPreferences,
 * and W3DDisplay drift out of sync.
 */
class DisplaySettingsManager : public SubsystemInterface
{
public:
    DisplaySettingsManager();
    virtual ~DisplaySettingsManager();

    // SubsystemInterface
    virtual void init() override;
    virtual void reset() override;
    virtual void update() override;

    // Current State (Single Source of Truth)
    Int getWidth() const { return m_width; }
    Int getHeight() const { return m_height; }
    Int getBitDepth() const { return m_bitDepth; }
    Bool isWindowed() const { return m_isWindowed; }
    Real getAspectRatio() const { return (m_height > 0) ? (Real)m_width / (Real)m_height : 1.333f; }

    // Persistence Integration (Handles Options.ini reading/writing)
    void loadFromIni();
    void saveToIni();
    
    void confirmResolutionChange();
    void revertResolutionChange();

    // Change Management
    /**
     * Requests a display mode change. 
     * Validates settings before updating the internal state and notifying listeners.
     */
    void requestResolutionChange(Int w, Int h, Bool windowed);
    
    // Callback Registration for UI and Renderer Refresh
    // This allows components like InGameUI to reposition themselves automatically.
    typedef void (*DisplayChangeCallback)(void* userData);
    void registerCallback(DisplayChangeCallback cb, void* userData);
    void unregisterCallback(DisplayChangeCallback cb);

private:
    Int m_width;
    Int m_height;
    Int m_bitDepth;
    Bool m_isWindowed;

    Int m_confirmedWidth;
    Int m_confirmedHeight;
    Bool m_confirmedIsWindowed;
    
    struct CallbackEntry {
        DisplayChangeCallback cb;
        void* userData;
    };
    std::vector<CallbackEntry> m_callbacks;

    /**
     * Notifies all registered listeners that the display mode has changed.
     */
    void notifyListeners();
    
    /**
     * Ensures requested resolutions are supported by the hardware and within engine limits.
     */
    void validateSettings(Int& w, Int& h);
};

extern DisplaySettingsManager* TheDisplaySettingsManager;
