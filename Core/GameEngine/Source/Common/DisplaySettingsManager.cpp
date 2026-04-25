/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
*/

#include "PreRTS.h"
#include "Common/DisplaySettingsManager.h"
#include "Common/GlobalData.h"
#include "Common/OptionPreferences.h"
#include "GameClient/Display.h"
#include "GameClient/HeaderTemplate.h"
#include "GameClient/Mouse.h"
#include "GameClient/Shell.h"

DisplaySettingsManager* TheDisplaySettingsManager = nullptr;

DisplaySettingsManager::DisplaySettingsManager()
    : m_width(800)
    , m_height(600)
    , m_bitDepth(32)
    , m_isWindowed(false)
    , m_confirmedWidth(800)
    , m_confirmedHeight(600)
    , m_confirmedIsWindowed(false)
{
    setName("DisplaySettingsManager");
}

DisplaySettingsManager::~DisplaySettingsManager()
{
}

void DisplaySettingsManager::init()
{
    loadFromIni();

    // Prioritize command line resolution if provided
    if (TheGlobalData->m_xResolution > 0 && TheGlobalData->m_yResolution > 0)
    {
        m_width = TheGlobalData->m_xResolution;
        m_height = TheGlobalData->m_yResolution;
    }

    // Prioritize -win command line flag
    if (TheGlobalData->m_windowed)
    {
        m_isWindowed = true;
    }

    m_confirmedWidth = m_width;
    m_confirmedHeight = m_height;
    m_confirmedIsWindowed = m_isWindowed;

    // Sync back to GlobalData so early window creation uses these
    TheWritableGlobalData->m_xResolution = m_width;
    TheWritableGlobalData->m_yResolution = m_height;
    TheWritableGlobalData->m_windowed = m_isWindowed;
}

void DisplaySettingsManager::reset()
{
    // No-op for now
}

void DisplaySettingsManager::update()
{
    // No-op for now
}

void DisplaySettingsManager::loadFromIni()
{
    OptionPreferences pref;
    if (!pref.loadFromIniFile())
    {
        return; // File doesn't exist or is invalid, keep defaults
    }
    
    // Load Resolution
    OptionPreferences::const_iterator it = pref.find("Resolution");
    if (it != pref.end())
    {
        Int w, h;
        if (sscanf(it->second.str(), "%d %d", &w, &h) == 2)
        {
            validateSettings(w, h);
            m_width = w;
            m_height = h;
        }
    }

    // Load Windowed Mode
    it = pref.find("Windowed");
    if (it != pref.end())
    {
        m_isWindowed = (stricmp(it->second.str(), "yes") == 0);
    }
}

void DisplaySettingsManager::saveToIni()
{
    OptionPreferences pref;
    pref.loadFromIniFile(); // Load existing first so we don't wipe other settings
    
    AsciiString resStr;
    resStr.format("%d %d", m_width, m_height);
    pref["Resolution"] = resStr;
    
    pref["Windowed"] = m_isWindowed ? "yes" : "no";
    
    pref.write();
}

void DisplaySettingsManager::confirmResolutionChange()
{
    m_confirmedWidth = m_width;
    m_confirmedHeight = m_height;
    m_confirmedIsWindowed = m_isWindowed;
    saveToIni();
}

void DisplaySettingsManager::revertResolutionChange()
{
    requestResolutionChange(m_confirmedWidth, m_confirmedHeight, m_confirmedIsWindowed);
}

void DisplaySettingsManager::requestResolutionChange(Int w, Int h, Bool windowed)
{
    validateSettings(w, h);

    if (m_width == w && m_height == h && m_isWindowed == windowed)
        return;

    if (TheDisplay->setDisplayMode(w, h, m_bitDepth, windowed))
    {
        m_width = w;
        m_height = h;
        m_isWindowed = windowed;

        // Update GlobalData for legacy compatibility
        TheWritableGlobalData->m_xResolution = m_width;
        TheWritableGlobalData->m_yResolution = m_height;
        TheWritableGlobalData->m_windowed = m_isWindowed;

        // Engine-wide notifications
        if (TheHeaderTemplateManager) TheHeaderTemplateManager->onResolutionChanged();
        if (TheMouse)                 TheMouse->onResolutionChanged();
        if (TheShell)                 TheShell->recreateWindowLayouts();

        notifyListeners();
    }
}

void DisplaySettingsManager::registerCallback(DisplayChangeCallback cb, void* userData)
{
    CallbackEntry entry = { cb, userData };
    m_callbacks.push_back(entry);
}

void DisplaySettingsManager::unregisterCallback(DisplayChangeCallback cb)
{
    m_callbacks.erase(
        std::remove_if(m_callbacks.begin(), m_callbacks.end(),
            [cb](const CallbackEntry& entry) { return entry.cb == cb; }),
        m_callbacks.end());
}

void DisplaySettingsManager::notifyListeners()
{
    for (auto& entry : m_callbacks)
    {
        if (entry.cb)
        {
            entry.cb(entry.userData);
        }
    }
}

void DisplaySettingsManager::validateSettings(Int& w, Int& h)
{
    // Ensure we don't try to set a resolution smaller than the engine's hardcoded minimums
    if (w < 800) w = 800;
    if (h < 600) h = 600;

    // @todo: Add hardware enumeration checks here later to ensure the monitor supports it.
}
