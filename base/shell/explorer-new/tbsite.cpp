/*
 * ReactOS Explorer
 *
 * Copyright 2006 - 2007 Thomas Weidenmueller <w3seek@reactos.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "precomp.h"

#include <shdeprecated.h>

#include "undoc.h"

/*****************************************************************************
 ** ITrayBandSite ************************************************************
 *****************************************************************************/

// WARNING: Can't use ATL for this class due to our ATL not fully supporting the AGGREGATION functions needed for this class to be an "outer" class
// it works just fine this way.
class ITrayBandSiteImpl :
    public ITrayBandSite,
    public IBandSite,
    public IBandSiteStreamCallback
    /* TODO: IWinEventHandler */
{
    volatile LONG m_RefCount;

    CComPtr<ITrayWindow> Tray;

    CComPtr<IUnknown> punkInner;
    CComPtr<IBandSite> BandSite;
    CComPtr<ITaskBand> TaskBand;
    CComPtr<IWinEventHandler> WindowEventHandler;
    CComPtr<IContextMenu> ContextMenu;

    HWND hWndRebar;

    union
    {
        DWORD dwFlags;
        struct
        {
            DWORD Locked : 1;
        };
    };

public:

    virtual ULONG STDMETHODCALLTYPE AddRef()
    {
        return InterlockedIncrement(&m_RefCount);
    }

    virtual ULONG STDMETHODCALLTYPE Release()
    {
        ULONG Ret = InterlockedDecrement(&m_RefCount);

        if (Ret == 0)
            delete this;

        return Ret;
    }

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(IN REFIID riid, OUT LPVOID *ppvObj)
    {
        if (ppvObj == NULL)
            return E_POINTER;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IBandSiteStreamCallback))
        {
            // return IBandSiteStreamCallback's IUnknown
            *ppvObj = static_cast<IBandSiteStreamCallback*>(this);
        }
        else if (IsEqualIID(riid, IID_IBandSite))
        {
            *ppvObj = static_cast<IBandSite*>(this);
        }
        else if (IsEqualIID(riid, IID_IWinEventHandler))
        {
            TRACE("ITaskBandSite: IWinEventHandler queried!\n");
            *ppvObj = NULL;
            return E_NOINTERFACE;
        }
        else if (punkInner != NULL)
        {
            return punkInner->QueryInterface(riid, ppvObj);
        }
        else
        {
            *ppvObj = NULL;
            return E_NOINTERFACE;
        }

        AddRef();
        return S_OK;
    }

public:
    ITrayBandSiteImpl() :
        m_RefCount(0),
        hWndRebar(NULL)
    {
    }

    virtual ~ITrayBandSiteImpl() { }

    virtual HRESULT STDMETHODCALLTYPE OnLoad(
        IN OUT IStream *pStm,
        IN REFIID riid,
        OUT PVOID *pvObj)
    {
        LARGE_INTEGER liPosZero;
        ULARGE_INTEGER liCurrent;
        CLSID clsid;
        ULONG ulRead;
        HRESULT hRet;

        /* NOTE: Callback routine called by the shell while loading the task band
                 stream. We use it to intercept the default behavior when the task
                 band is loaded from the stream.

                 NOTE: riid always points to IID_IUnknown! This is because the shell hasn't
                 read anything from the stream and therefore doesn't know what CLSID
                 it's dealing with. We'll have to find it out ourselves by reading
                 the GUID from the stream. */

        /* Read the current position of the stream, we'll have to reset it everytime
           we read a CLSID that's not the task band... */
        ZeroMemory(&liPosZero,
            sizeof(liPosZero));
        hRet = pStm->Seek(liPosZero, STREAM_SEEK_CUR, &liCurrent);

        if (SUCCEEDED(hRet))
        {
            /* Now let's read the CLSID from the stream and see if it's our task band */
            hRet = pStm->Read(&clsid, (ULONG)sizeof(clsid), &ulRead);

            if (SUCCEEDED(hRet) && ulRead == sizeof(clsid))
            {
                if (IsEqualGUID(clsid, CLSID_ITaskBand))
                {
                    ASSERT(TaskBand != NULL);
                    /* We're trying to load the task band! Let's create it... */

                    hRet = TaskBand->QueryInterface(
                        riid,
                        pvObj);
                    if (SUCCEEDED(hRet))
                    {
                        /* Load the stream */
                        TRACE("IBandSiteStreamCallback::OnLoad intercepted the task band CLSID!\n");
                    }

                    return hRet;
                }
            }
        }

        /* Reset the position and let the shell do all the work for us */
        hRet = pStm->Seek(
            *(LARGE_INTEGER*) &liCurrent,
            STREAM_SEEK_SET,
            NULL);
        if (SUCCEEDED(hRet))
        {
            /* Let the shell handle everything else for us :) */
            hRet = OleLoadFromStream(pStm,
                riid,
                pvObj);
        }

        if (!SUCCEEDED(hRet))
        {
            TRACE("IBandSiteStreamCallback::OnLoad(0x%p, 0x%p, 0x%p) returns 0x%x\n", pStm, riid, pvObj, hRet);
        }

        return hRet;
    }

    virtual HRESULT STDMETHODCALLTYPE OnSave(
        IN OUT IUnknown *pUnk,
        IN OUT IStream *pStm)
    {
        /* NOTE: Callback routine called by the shell while saving the task band
                 stream. We use it to intercept the default behavior when the task
                 band is saved to the stream */
        /* FIXME: Implement */
        TRACE("IBandSiteStreamCallback::OnSave(0x%p, 0x%p) returns E_NOTIMPL\n", pUnk, pStm);
        return E_NOTIMPL;
    }

    virtual HRESULT STDMETHODCALLTYPE IsTaskBand(
        IN IUnknown *punk)
    {
        return IsSameObject((IUnknown *) BandSite,
            punk);
    }

    virtual HRESULT STDMETHODCALLTYPE ProcessMessage(
        IN HWND hWnd,
        IN UINT uMsg,
        IN WPARAM wParam,
        IN LPARAM lParam,
        OUT LRESULT *plResult)
    {
        HRESULT hRet;

        ASSERT(hWndRebar != NULL);

        /* Custom task band behavior */
        switch (uMsg)
        {
        case WM_NOTIFY:
        {
            const NMHDR *nmh = (const NMHDR *) lParam;

            if (nmh->hwndFrom == hWndRebar)
            {
                switch (nmh->code)
                {
                case NM_NCHITTEST:
                {
                    LPNMMOUSE nmm = (LPNMMOUSE) lParam;

                    if (nmm->dwHitInfo == RBHT_CLIENT || nmm->dwHitInfo == RBHT_NOWHERE ||
                        nmm->dwItemSpec == (DWORD_PTR) -1)
                    {
                        /* Make the rebar control appear transparent so the user
                           can drag the tray window */
                        *plResult = HTTRANSPARENT;
                    }
                    return S_OK;
                }

                case RBN_MINMAX:
                    /* Deny if an Administrator disabled this "feature" */
                    *plResult = (SHRestricted(REST_NOMOVINGBAND) != 0);
                    return S_OK;
                }
            }

            //TRACE("ITrayBandSite::ProcessMessage: WM_NOTIFY for 0x%p, From: 0x%p, Code: NM_FIRST-%u...\n", hWnd, nmh->hwndFrom, NM_FIRST - nmh->code);
            break;
        }
        }

        /* Forward to the shell's IWinEventHandler interface to get the default shell behavior! */
        if (!WindowEventHandler)
            return E_FAIL;

        /*TRACE("Calling IWinEventHandler::ProcessMessage(0x%p, 0x%x, 0x%p, 0x%p, 0x%p) hWndRebar=0x%p\n", hWnd, uMsg, wParam, lParam, plResult, hWndRebar);*/
        hRet = WindowEventHandler->OnWinEvent(
            hWnd,
            uMsg,
            wParam,
            lParam,
            plResult);
        if (FAILED_UNEXPECTEDLY(hRet))
        {
            if (uMsg == WM_NOTIFY)
            {
                const NMHDR *nmh = (const NMHDR *) lParam;
                ERR("ITrayBandSite->IWinEventHandler::ProcessMessage: WM_NOTIFY for 0x%p, From: 0x%p, Code: NM_FIRST-%u returned 0x%x\n", hWnd, nmh->hwndFrom, NM_FIRST - nmh->code, hRet);
            }
            else
            {
                ERR("ITrayBandSite->IWinEventHandler::ProcessMessage(0x%p,0x%x,0x%p,0x%p,0x%p->0x%p) returned: 0x%x\n", hWnd, uMsg, wParam, lParam, plResult, *plResult, hRet);
            }
        }

        return hRet;
    }

    virtual HRESULT STDMETHODCALLTYPE AddContextMenus(
        IN HMENU hmenu,
        IN UINT indexMenu,
        IN UINT idCmdFirst,
        IN UINT idCmdLast,
        IN UINT uFlags,
        OUT IContextMenu **ppcm)
    {
        IShellService *pSs;
        HRESULT hRet;

        if (ContextMenu == NULL)
        {
            /* Cache the context menu so we don't need to CoCreateInstance all the time... */
            hRet = CoCreateInstance(CLSID_IShellBandSiteMenu, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARG(IShellService, &pSs));
            TRACE("CoCreateInstance(CLSID_IShellBandSiteMenu) for IShellService returned: 0x%x\n", hRet);
            if (!SUCCEEDED(hRet))
                return hRet;

            hRet = pSs->SetOwner((IBandSite*)this);
            if (!SUCCEEDED(hRet))
            {
                pSs->Release();
                return hRet;
            }

            hRet = pSs->QueryInterface(IID_PPV_ARG(IContextMenu, &ContextMenu));

            pSs->Release();

            if (!SUCCEEDED(hRet))
                return hRet;
        }

        if (ppcm != NULL)
        {
            ContextMenu->AddRef();
            *ppcm = ContextMenu;
        }

        /* Add the menu items */
        return ContextMenu->QueryContextMenu(
            hmenu,
            indexMenu,
            idCmdFirst,
            idCmdLast,
            uFlags);
    }

    virtual HRESULT STDMETHODCALLTYPE Lock(
        IN BOOL bLock)
    {
        BOOL bPrevLocked = Locked;
        BANDSITEINFO bsi;
        HRESULT hRet;

        ASSERT(BandSite != NULL);

        if (bPrevLocked != bLock)
        {
            Locked = bLock;

            bsi.dwMask = BSIM_STYLE;
            bsi.dwStyle = (Locked ? BSIS_LOCKED | BSIS_NOGRIPPER : BSIS_AUTOGRIPPER);

            hRet = BandSite->SetBandSiteInfo(&bsi);
            if (SUCCEEDED(hRet))
            {
                hRet = Update();
            }

            return hRet;
        }

        return S_FALSE;
    }

    /*******************************************************************/

    virtual HRESULT STDMETHODCALLTYPE AddBand(
        IN IUnknown *punk)
    {
        IOleCommandTarget *pOct;
        HRESULT hRet;

        hRet = punk->QueryInterface(IID_PPV_ARG(IOleCommandTarget, &pOct));
        if (SUCCEEDED(hRet))
        {
            /* Send the DBID_DELAYINIT command to initialize the band to be added */
            /* FIXME: Should be delayed */
            pOct->Exec(
                &IID_IDeskBand,
                DBID_DELAYINIT,
                0,
                NULL,
                NULL);

            pOct->Release();
        }

        return BandSite->AddBand(
            punk);
    }

    virtual HRESULT STDMETHODCALLTYPE EnumBands(
        IN UINT uBand,
        OUT DWORD *pdwBandID)
    {
        return BandSite->EnumBands(
            uBand,
            pdwBandID);
    }

    virtual HRESULT STDMETHODCALLTYPE QueryBand(
        IN DWORD dwBandID,
        OUT IDeskBand **ppstb,
        OUT DWORD *pdwState,
        OUT LPWSTR pszName,
        IN int cchName)
    {
        HRESULT hRet;
        IDeskBand *pstb = NULL;

        hRet = BandSite->QueryBand(
            dwBandID,
            &pstb,
            pdwState,
            pszName,
            cchName);

        if (SUCCEEDED(hRet))
        {
            hRet = IsSameObject(pstb, TaskBand);
            if (hRet == S_OK)
            {
                /* Add the BSSF_UNDELETEABLE flag to pdwState because the task bar band shouldn't be deletable */
                if (pdwState != NULL)
                    *pdwState |= BSSF_UNDELETEABLE;
            }
            else if (!SUCCEEDED(hRet))
            {
                pstb->Release();
                pstb = NULL;
            }

            if (ppstb != NULL)
                *ppstb = pstb;
        }
        else if (ppstb != NULL)
            *ppstb = NULL;

        return hRet;
    }

    virtual HRESULT STDMETHODCALLTYPE SetBandState(
        IN DWORD dwBandID,
        IN DWORD dwMask,
        IN DWORD dwState)
    {
        return BandSite->SetBandState(dwBandID, dwMask, dwState);
    }

    virtual HRESULT STDMETHODCALLTYPE RemoveBand(
        IN DWORD dwBandID)
    {
        return BandSite->RemoveBand(dwBandID);
    }

    virtual HRESULT STDMETHODCALLTYPE GetBandObject(
        IN DWORD dwBandID,
        IN REFIID riid,
        OUT VOID **ppv)
    {
        return BandSite->GetBandObject(dwBandID, riid, ppv);
    }

    virtual HRESULT STDMETHODCALLTYPE SetBandSiteInfo(
        IN const BANDSITEINFO *pbsinfo)
    {
        return BandSite->SetBandSiteInfo(pbsinfo);
    }

    virtual HRESULT STDMETHODCALLTYPE GetBandSiteInfo(
        IN OUT BANDSITEINFO *pbsinfo)
    {
        return BandSite->GetBandSiteInfo(pbsinfo);
    }

    virtual BOOL HasTaskBand()
    {
        ASSERT(TaskBand != NULL);

        return SUCCEEDED(TaskBand->GetRebarBandID(
            NULL));
    }

    virtual HRESULT AddTaskBand()
    {
#if 0
        /* FIXME: This is the code for the simple taskbar */
        IObjectWithSite *pOws;
        HRESULT hRet;

        hRet = TaskBand->QueryInterface(
            &IID_IObjectWithSite,
            (PVOID*) &pOws);
        if (SUCCEEDED(hRet))
        {
            hRet = pOws->SetSite(
                (IUnknown *)TaskBand);

            pOws->Release();
        }

        return hRet;
#else
        if (!HasTaskBand())
        {
            return BandSite->AddBand(TaskBand);
        }

        return S_OK;
#endif
    }

    virtual HRESULT Update()
    {
        IOleCommandTarget *pOct;
        HRESULT hRet;

        hRet = punkInner->QueryInterface(IID_PPV_ARG(IOleCommandTarget, &pOct));
        if (SUCCEEDED(hRet))
        {
            /* Send the DBID_BANDINFOCHANGED command to update the band site */
            hRet = pOct->Exec(
                &IID_IDeskBand,
                DBID_BANDINFOCHANGED,
                0,
                NULL,
                NULL);

            pOct->Release();
        }

        return hRet;
    }

    virtual VOID BroadcastOleCommandExec(const GUID *pguidCmdGroup,
        DWORD nCmdID,
        DWORD nCmdExecOpt,
        VARIANTARG *pvaIn,
        VARIANTARG *pvaOut)
    {
        IOleCommandTarget *pOct;
        DWORD dwBandID;
        UINT uBand = 0;

        /* Enumerate all bands */
        while (SUCCEEDED(BandSite->EnumBands(uBand, &dwBandID)))
        {
            if (SUCCEEDED(BandSite->GetBandObject(dwBandID, IID_PPV_ARG(IOleCommandTarget, &pOct))))
            {
                /* Execute the command */
                pOct->Exec(
                    pguidCmdGroup,
                    nCmdID,
                    nCmdExecOpt,
                    pvaIn,
                    pvaOut);

                pOct->Release();
            }

            uBand++;
        }
    }

    virtual HRESULT FinishInit()
    {
        /* Broadcast the DBID_FINISHINIT command */
        BroadcastOleCommandExec(&IID_IDeskBand, DBID_FINISHINIT, 0, NULL, NULL);

        return S_OK;
    }

    virtual HRESULT Show(
        IN BOOL bShow)
    {
        IDeskBarClient *pDbc;
        HRESULT hRet;

        hRet = BandSite->QueryInterface(IID_PPV_ARG(IDeskBarClient, &pDbc));
        if (SUCCEEDED(hRet))
        {
            hRet = pDbc->UIActivateDBC(
                bShow ? DBC_SHOW : DBC_HIDE);
            pDbc->Release();
        }

        return hRet;
    }

    virtual HRESULT LoadFromStream(IN OUT IStream *pStm)
    {
        IPersistStream *pPStm;
        HRESULT hRet;

        ASSERT(BandSite != NULL);

        /* We implement the undocumented COM interface IBandSiteStreamCallback
           that the shell will query so that we can intercept and custom-load
           the task band when it finds the task band's CLSID (which is internal).
           This way we can prevent the shell from attempting to CoCreateInstance
           the (internal) task band, resulting in a failure... */
        hRet = BandSite->QueryInterface(IID_PPV_ARG(IPersistStream, &pPStm));
        if (SUCCEEDED(hRet))
        {
            hRet = pPStm->Load(
                pStm);
            TRACE("->Load() returned 0x%x\n", hRet);
            pPStm->Release();
        }

        return hRet;
    }

    virtual IStream *
        GetUserBandsStream(IN DWORD grfMode)
    {
        HKEY hkStreams;
        IStream *Stream = NULL;

        if (RegCreateKey(hkExplorer,
            TEXT("Streams"),
            &hkStreams) == ERROR_SUCCESS)
        {
            Stream = SHOpenRegStream(hkStreams,
                TEXT("Desktop"),
                TEXT("TaskbarWinXP"),
                grfMode);

            RegCloseKey(hkStreams);
        }

        return Stream;
    }

    virtual IStream *
        GetDefaultBandsStream(IN DWORD grfMode)
    {
        HKEY hkStreams;
        IStream *Stream = NULL;

        if (RegCreateKey(HKEY_LOCAL_MACHINE,
            TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Streams"),
            &hkStreams) == ERROR_SUCCESS)
        {
            Stream = SHOpenRegStream(hkStreams,
                TEXT("Desktop"),
                TEXT("Default Taskbar"),
                grfMode);

            RegCloseKey(hkStreams);
        }

        return Stream;
    }

    virtual HRESULT Load()
    {
        IStream *pStm;
        HRESULT hRet;

        /* Try to load the user's settings */
        pStm = GetUserBandsStream(STGM_READ);
        if (pStm != NULL)
        {
            hRet = LoadFromStream(pStm);

            TRACE("Loaded user bands settings: 0x%x\n", hRet);
            pStm->Release();
        }
        else
            hRet = E_FAIL;

        /* If the user's settings couldn't be loaded, try with
           default settings (ie. when the user logs in for the
           first time! */
        if (!SUCCEEDED(hRet))
        {
            pStm = GetDefaultBandsStream(STGM_READ);
            if (pStm != NULL)
            {
                hRet = LoadFromStream(pStm);

                TRACE("Loaded default user bands settings: 0x%x\n", hRet);
                pStm->Release();
            }
            else
                hRet = E_FAIL;
        }

        return hRet;
    }

    HRESULT _Init(IN OUT ITrayWindow *tray, OUT HWND *phWndRebar, OUT HWND *phwndTaskSwitch)
    {
        IDeskBarClient *pDbc;
        IDeskBand *pDb;
        IOleWindow *pOw;
        HRESULT hRet;

        *phWndRebar = NULL;
        *phwndTaskSwitch = NULL;

        Tray = tray;

        /* Create a RebarBandSite provided by the shell */
        hRet = CoCreateInstance(CLSID_RebarBandSite,
            static_cast<IBandSite*>(this),
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARG(IUnknown, &punkInner));
        if (!SUCCEEDED(hRet))
        {
            return hRet;
        }

        hRet = punkInner->QueryInterface(IID_PPV_ARG(IBandSite, &BandSite));
        if (!SUCCEEDED(hRet))
        {
            return hRet;
        }

        hRet = punkInner->QueryInterface(IID_PPV_ARG(IWinEventHandler, &WindowEventHandler));
        if (!SUCCEEDED(hRet))
        {
            return hRet;
        }

        TaskBand = CreateTaskBand(Tray);
        if (TaskBand != NULL)
        {
            /* Add the task band to the site */
            hRet = BandSite->QueryInterface(IID_PPV_ARG(IDeskBarClient, &pDbc));
            if (SUCCEEDED(hRet))
            {
                hRet = TaskBand->QueryInterface(IID_PPV_ARG(IOleWindow, &pOw));
                if (SUCCEEDED(hRet))
                {
                    /* We cause IDeskBarClient to create the rebar control by passing the new
                       task band to it. The band reports the tray window handle as window handle
                       so that IDeskBarClient knows the parent window of the Rebar control that
                       it wants to create. */
                    hRet = pDbc->SetDeskBarSite(pOw);

                    if (SUCCEEDED(hRet))
                    {
                        /* The Rebar control is now created, we can query the window handle */
                        hRet = pDbc->GetWindow(&hWndRebar);

                        if (SUCCEEDED(hRet))
                        {
                            /* We need to manually remove the RBS_BANDBORDERS style! */
                            SetWindowStyle(hWndRebar, RBS_BANDBORDERS, 0);
                        }
                    }

                    pOw->Release();
                }

                if (SUCCEEDED(hRet))
                {
                    DWORD dwMode = 0;

                    /* Set the Desk Bar mode to the current one */

                    /* FIXME: We need to set the mode (and update) whenever the user docks
                              the tray window to another monitor edge! */

                    if (!Tray->IsHorizontal())
                        dwMode = DBIF_VIEWMODE_VERTICAL;

                    hRet = pDbc->SetModeDBC(dwMode);
                }

                pDbc->Release();
            }

            /* Load the saved state of the task band site */
            /* FIXME: We should delay loading shell extensions, also see DBID_DELAYINIT */
            Load();

            /* Add the task bar band if it hasn't been added already */
            hRet = AddTaskBand();
            if (SUCCEEDED(hRet))
            {
                hRet = TaskBand->QueryInterface(IID_PPV_ARG(IDeskBand, &pDb));
                if (SUCCEEDED(hRet))
                {
                    hRet = pDb->GetWindow(phwndTaskSwitch);
                    if (!SUCCEEDED(hRet))
                        *phwndTaskSwitch = NULL;

                    pDb->Release();
                }
            }

            /* Should we send this after showing it? */
            Update();

            /* FIXME: When should we send this? Does anyone care anyway? */
            FinishInit();

            /* Activate the band site */
            Show(
                TRUE);
        }

        *phWndRebar = hWndRebar;

        return S_OK;
    }
        };
/*******************************************************************/

ITrayBandSite *
CreateTrayBandSite(IN OUT ITrayWindow *Tray,
OUT HWND *phWndRebar,
OUT HWND *phWndTaskSwitch)
{
    HRESULT hr;

    ITrayBandSiteImpl * tb = new ITrayBandSiteImpl();

    if (!tb)
        return NULL;

    tb->AddRef();

    hr = tb->_Init(Tray, phWndRebar, phWndTaskSwitch);
    if (FAILED_UNEXPECTEDLY(hr))
        tb->Release();

    return tb;
}
