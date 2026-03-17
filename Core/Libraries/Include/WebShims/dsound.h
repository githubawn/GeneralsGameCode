#pragma once
#include "WebWin32.h"

typedef struct _WAVEFORMATEX {
    WORD wFormatTag;
    WORD nChannels;
    DWORD nSamplesPerSec;
    DWORD nAvgBytesPerSec;
    WORD nBlockAlign;
    WORD wBitsPerSample;
    WORD cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX, *LPWAVEFORMATEX;

typedef struct _WAVEFORMATEXTENSIBLE {
    WAVEFORMATEX Format;
    union {
        WORD wValidBitsPerSample;
        WORD wSamplesPerPixel;
        WORD wReserved;
    } Samples;
    DWORD dwChannelMask;
    GUID SubFormat;
} WAVEFORMATEXTENSIBLE, *PWAVEFORMATEXTENSIBLE;

typedef struct IDirectSound *LPDIRECTSOUND;
typedef struct IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;

struct IDirectSoundBuffer : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE GetCaps(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPosition(DWORD*, DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFormat(WAVEFORMATEX*, DWORD, DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetVolume(LONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetPan(LONG*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetFrequency(DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStatus(DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(LPDIRECTSOUND, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Lock(DWORD, DWORD, void**, DWORD*, void**, DWORD*, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Play(DWORD, DWORD, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCurrentPosition(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFormat(const WAVEFORMATEX*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetVolume(LONG) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetPan(LONG) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetFrequency(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unlock(void*, DWORD, void*, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Restore() = 0;
};

struct IDirectSound : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE CreateSoundBuffer(const void*, LPDIRECTSOUNDBUFFER*, IUnknown*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCaps(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE DuplicateSoundBuffer(LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND, DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Compact() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetSpeakerConfig(DWORD*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetSpeakerConfig(DWORD) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(const GUID*) = 0;
};

#define DSSCL_NORMAL                0x00000001
#define DSSCL_PRIORITY              0x00000002
#define DSSCL_EXCLUSIVE             0x00000003
#define DSSCL_WRITEPRIMARY          0x00000004

#define DS_OK 0

inline HRESULT DirectSoundCreate(const GUID* pcGuidDevice, LPDIRECTSOUND* ppDS, IUnknown* pUnkOuter) { return -1; }
