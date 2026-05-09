#pragma once
#include <string>

// ---------------------------------------------------------------------------
// WebMessageHandler
//
// Handles messages sent from the WebView2 JavaScript UI to C++.
//
// Protocol (pipe-delimited):
//   getAll                                  → send all expansions to UI
//   insert|token|value|tags|type            → add new expansion
//   update|id|token|value|tags|type         → edit existing expansion
//   delete|id                               → remove expansion
//   search|query                            → search expansions, send results
//   settings|{jsonObject}                   → apply user settings
//
// All responses are sent back via SafeExecuteScript calling JavaScript
// functions: window.loadFromNative([...])
// ---------------------------------------------------------------------------

// Parse and handle a message from the WebView2 UI.
// Logs and ignores malformed messages.
void HandleMessage(const std::wstring& msg);

// Execute JavaScript in the WebView2 context safely.
// Logs error if execution fails. Returns true on success.
bool SafeExecuteScript(const std::wstring& script);

// Query all expansions from the database and send to the UI.
void SendAllExpansions();