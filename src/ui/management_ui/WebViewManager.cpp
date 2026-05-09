#include "WebViewManager.h"
#include "WebViewBridge.h"
#include "WebMessageHandler.h"
#include "WebResources.h"
#include "WebSettings.h"
#include "AppLog.h"

#include <windows.h>
#include <string>
#include <WebView2.h>

using namespace Microsoft::WRL;

// ===========================================================================
// GLOBALS
// ===========================================================================

HWND                             g_uiWindow = nullptr;
ComPtr<ICoreWebView2Controller>  g_controller;
ComPtr<ICoreWebView2>            g_webview;

// ===========================================================================
// WEBVIEW INITIALIZATION
// ===========================================================================

void InitWebView(HWND hwnd)
{
    AppLog::Info(L"InitWebView: Starting WebView2 initialization");

    CreateCoreWebView2EnvironmentWithOptions(
        nullptr,  // browserExecutableFolder (use default)
        nullptr,  // userDataFolder (use default)
        nullptr,  // additionalBrowserArguments
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
            {
                // -------------------------------------------------------
                // Environment creation callback
                // -------------------------------------------------------
                if (FAILED(hr))
                {
                    AppLog::Error(L"CreateCoreWebView2Environment failed: 0x" +
                        std::to_wstring((unsigned long)hr));
                    MessageBoxW(nullptr,
                        L"WebView2 initialization failed.\n\n"
                        L"Please ensure WebView2 runtime is installed.",
                        L"Error",
                        MB_OK | MB_ICONERROR);
                    return E_FAIL;
                }

                if (!env)
                {
                    AppLog::Error(L"WebView2Environment pointer is null");
                    return E_POINTER;
                }

                AppLog::Info(L"InitWebView: WebView2 environment created");

                // Now create the controller
                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT hr, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            // -----------------------------------------------
                            // Controller creation callback
                            // -----------------------------------------------
                            if (FAILED(hr))
                            {
                                AppLog::Error(L"CreateCoreWebView2Controller failed: 0x" +
                                    std::to_wstring((unsigned long)hr));
                                return E_FAIL;
                            }

                            if (!controller)
                            {
                                AppLog::Error(L"WebView2Controller pointer is null");
                                return E_POINTER;
                            }

                            AppLog::Info(L"InitWebView: WebView2 controller created");

                            // Store the controller and get the webview
                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);

                            if (!g_webview)
                            {
                                AppLog::Error(L"Failed to get CoreWebView2 interface");
                                return E_POINTER;
                            }

                            // Set initial bounds
                            RECT bounds{};
                            GetClientRect(hwnd, &bounds);
                            g_controller->put_Bounds(bounds);

                            AppLog::Info(L"InitWebView: bounds set");

                            // -----------------------------------------------
                            // Register: JS → C++ message bridge
                            // -----------------------------------------------
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*,
                                        ICoreWebView2WebMessageReceivedEventArgs* args)
                                    -> HRESULT
                                    {
                                        LPWSTR raw = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw)
                                        {
                                            std::wstring msg = raw;
                                            CoTaskMemFree(raw);

                                            // Handle the message from JavaScript
                                            HandleMessage(msg);
                                        }
                                        return S_OK;
                                    }).Get(),
                                        nullptr);

                            AppLog::Info(L"InitWebView: message bridge registered");

                            // -----------------------------------------------
                            // Load the HTML UI
                            // -----------------------------------------------
                            std::wstring html = BuildHTML();
                            if (html.empty())
                            {
                                AppLog::Error(L"BuildHTML returned empty string");
                                return E_FAIL;
                            }

                            g_webview->NavigateToString(html.c_str());
                            AppLog::Info(L"InitWebView: navigating to HTML");

                            // -----------------------------------------------
                            // Register: after page loads, populate with data
                            // -----------------------------------------------
                            g_webview->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* sender,
                                        ICoreWebView2NavigationCompletedEventArgs*)
                                    -> HRESULT
                                    {
                                        AppLog::Info(L"InitWebView: navigation completed");

                                        // Send all expansions from database
                                        SafeExecuteScript(
                                            L"window.chrome.webview.postMessage('getAll');");

                                        // Send list of installed applications
                                        SendInstalledAppsToUI(sender);

                                        // Send current user settings
                                        SafeExecuteScript(BuildSettingsScript());

                                        AppLog::Info(L"InitWebView: data populated");

                                        return S_OK;
                                    }).Get(),
                                        nullptr);

                            return S_OK;
                        }).Get());

                return S_OK;
            }).Get());

    AppLog::Info(L"InitWebView: async initialization started");
}

// ===========================================================================
// WINDOW PROCEDURE
// ===========================================================================

LRESULT CALLBACK WebUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        // Keep WebView2 resized to fill the window
        if (g_controller)
        {
            RECT bounds{};
            GetClientRect(hwnd, &bounds);
            g_controller->put_Bounds(bounds);
        }
        return 0;

    case WM_CLOSE:
        // Close the window cleanly
        PostQuitMessage(0);
        return 0;

    case WM_DESTROY:
        // Clean up WebView2
        if (g_controller)
        {
            g_controller->Close();
            g_controller = nullptr;
            g_webview = nullptr;
        }
        g_uiWindow = nullptr;
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}