#include "WebViewManager.h"
#include <string>
#include <WebMessageHandler.h>
#include <WebResources.h>
#include <WebSettings.h>
using namespace Microsoft::WRL;


void SendInstalledAppsToUI(ICoreWebView2* webview);

HWND                             g_uiWindow = nullptr;
ComPtr<ICoreWebView2Controller>  g_controller;
ComPtr<ICoreWebView2>            g_webview;


// ---------------------------------------------------------------------------
// WEBVIEW INIT
// ---------------------------------------------------------------------------



void InitWebView(HWND hwnd)
{
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [hwnd](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT
            {
                if (FAILED(hr) || !env)
                {
                    MessageBoxW(nullptr, L"WebView2 init failed", L"Error", MB_OK);
                    return E_FAIL;
                }

                env->CreateCoreWebView2Controller(hwnd,
                    Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [hwnd](HRESULT, ICoreWebView2Controller* controller) -> HRESULT
                        {
                            if (!controller) return S_OK;

                            g_controller = controller;
                            g_controller->get_CoreWebView2(&g_webview);

                            RECT bounds;
                            GetClientRect(hwnd, &bounds);
                            g_controller->put_Bounds(bounds);

                            // JS → C++ bridge
                            g_webview->add_WebMessageReceived(
                                Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [](ICoreWebView2*,
                                        ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                    {


                                        LPWSTR raw = nullptr;
                                        if (SUCCEEDED(args->TryGetWebMessageAsString(&raw)) && raw)
                                        {
                                            std::wstring msgW = raw;
                                            CoTaskMemFree(raw);

                                            std::string s(msgW.begin(), msgW.end()); // 🔴 NOW s exists

                                            HandleMessage(msgW);
                                        }
                                        return S_OK;
                                    }).Get(),
                                        nullptr);

                            // Load HTML
                            std::wstring html = BuildHTML();
                            g_webview->NavigateToString(html.c_str());

                            // After navigation: send all data + current settings
                            g_webview->add_NavigationCompleted(
                                Callback<ICoreWebView2NavigationCompletedEventHandler>(
                                    [](ICoreWebView2* sender,
                                        ICoreWebView2NavigationCompletedEventArgs*) -> HRESULT
                                    {
                                        // Push all expansions
                                        SafeExecuteScript(
                                            L"window.chrome.webview.postMessage('getAll');");
                                        SendInstalledAppsToUI(sender);
                                        // Push current settings into the UI
                                        SafeExecuteScript(BuildSettingsScript());
                                        return S_OK;
                                    }).Get(),
                                        nullptr);

                            return S_OK;
                        }).Get());
                return S_OK;
            }).Get());
}

// ---------------------------------------------------------------------------
// WINDOW PROCEDURE
// ---------------------------------------------------------------------------

LRESULT CALLBACK WebUIProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_SIZE:
        if (g_controller)
        {
            RECT bounds;
            GetClientRect(hwnd, &bounds);
            g_controller->put_Bounds(bounds);
        }
        return 0;

    case WM_DESTROY:
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