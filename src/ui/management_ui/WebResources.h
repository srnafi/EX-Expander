#pragma once
#include <string>

// ---------------------------------------------------------------------------
// ResourceBuilder
//
// Loads HTML, CSS, and JavaScript from embedded resources (compiled into
// the exe via the .rc file) and combines them into a single HTML document
// for the WebView2 control.
//
// Resources used (defined in resource.h):
//   IDR_HTML  –  base HTML structure
//   IDR_CSS   –  styles (injected into <head>)
//   IDR_JS    –  JavaScript (injected before </body>)
//
// If any resource fails to load, the function logs an error and returns
// an empty string or partial HTML (which will fail to render correctly).
// ---------------------------------------------------------------------------

// Load a text resource by ID. Returns empty string on failure.
// Assumes the resource is UTF-8 encoded (standard for web assets).
std::wstring LoadResourceText(int resourceId);

// Build the complete HTML document by injecting CSS and JS into the
// base HTML template. Returns empty string if base HTML fails to load.
std::wstring BuildHTML();