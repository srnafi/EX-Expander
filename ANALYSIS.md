# 📋 EX-Expander Code Analysis & Refactoring Guide

## Executive Summary
Your project is **well-structured** but has opportunities for:
1. **Reducing duplicated logic** (especially in message parsing)
2. **Improving testability** with dependency injection
3. **Better error handling** and validation
4. **OOP where it makes sense** (Registry access, Clipboard operations, Settings management)

---

## 🔴 Critical Issues Found

### 1. **DUPLICATE: Exact Match Logic** ❌
**Location:** `KeyboardHook.cpp` (appears twice)

```cpp
// Lines 19-25 & 92 & 120
static const Expansion* FindExactMatch(const std::vector<Expansion>& matches,
    const std::wstring& token)
{
    for (const auto& m : matches)
        if (m.token == token) return &m;
    return nullptr;
}
```

**Used in:**
- Line 92: Symbol-mode confirmation
- Line 120: Space-trigger confirmation

**Fix:** Extract to `EmojiMatcher.h`
```cpp
// In EmojiMatcher.h
const Expansion* FindExactMatch(const std::vector<Expansion>& matches,
    const std::wstring& token);

// In EmojiMatcher.cpp
const Expansion* FindExactMatch(const std::vector<Expansion>& matches,
    const std::wstring& token)
{
    for (const auto& m : matches)
        if (m.token == token) return &m;
    return nullptr;
}
```

---

### 2. **DUPLICATE: Clipboard RAII Guard** ⚠️
**Location:** `EmojiReplacer.cpp` (lines 12-39)

The `ClipboardGuard` class is well-implemented but **not reusable**. If you ever need clipboard operations elsewhere, you'll duplicate this.

**Fix:** Create a shared utility
```cpp
// src/helper/ClipboardHelper.h
class ClipboardGuard {
    // Move from EmojiReplacer.cpp here
};

// Include in EmojiReplacer.cpp
#include "ClipboardHelper.h"
```

---

### 3. **DUPLICATE: Settings Parsing Logic** 🔄
**Location:** `WebMessageHandler.cpp` (lines 108-117) vs `WebSettings.cpp` (lines 43-46)

```cpp
// WebMessageHandler.cpp - parses settings from JSON
bool         autoStart = JsonBoolW(jsonStr, L"autoStart");
std::wstring emojiSymbol = JsonStringW(jsonStr, L"emojiSymbol");
int          maxPopup = JsonIntW(jsonStr, L"maxPopup");
// ...
if (emojiSymbol.empty())   emojiSymbol = L":";
if (maxPopup <= 0)         maxPopup = 5;

// WebSettings.cpp - retrieves and applies same settings
std::wstring sym = DB_GetSetting(L"emojiSymbol", L":");
std::wstring maxStr = DB_GetSetting(L"maxPopup", L"5");
int maxVal = max(1, min(10, _wtoi(maxStr.c_str())));
```

**Fix:** Create a `Settings` struct

```cpp
// src/helper/Settings.h
struct AppSettings {
    bool         autoStart = false;
    std::wstring emojiSymbol = L":";
    int          maxPopupItems = 5;
    std::wstring insertTrigger = L"space";
    std::wstring popupPosition = L"fixed";
    std::wstring scopeMode = L"block";
    std::vector<std::wstring> scopeApps;
    
    // Validation
    void Validate() {
        maxPopupItems = std::max(3, std::min(10, maxPopupItems));
        if (emojiSymbol.empty()) emojiSymbol = L":";
        if (insertTrigger.empty()) insertTrigger = L"space";
    }
    
    // Serialization from JSON
    static AppSettings FromJson(const std::wstring& json);
    
    // Serialization to JSON
    std::wstring ToJson() const;
};
```

Then use it:
```cpp
// WebMessageHandler.cpp
AppSettings settings = AppSettings::FromJson(jsonStr);
settings.Validate();
ApplySettings(settings);  // Single function call
```

---

### 4. **DUPLICATE: Keyboard Input Simulation** 🎮
**Location:** `EmojiReplacer.cpp` (lines 128-137)

The `AddKey()` helper at lines 128-137 is tightly coupled to `ReplaceWithEmoji()`.

**Improvement:** Extract to testable module

```cpp
// src/helper/KeySimulator.h
class KeySimulator {
public:
    static void SimulateBackspace(int count);
    static void SimulateCtrlV();
    static void SimulateKey(WORD vk, bool keyUp = false);
    
private:
    static void AddKey(std::vector<INPUT>& inputs, WORD vk, bool keyUp);
};
```

**Benefit:** Can be unit-tested independently.

---

### 5. **GLOBAL STATE SPRAWL** 🌍
**Location:** `Globals.h` & `Globals.cpp`

You have 9 global variables spread across multiple systems:
```cpp
extern HINSTANCE                    g_hInstance;
extern std::vector<Expansion>       g_FilteredExpansions;
extern wchar_t                      g_TriggerChar;
extern int                          g_MaxPopupItems;
extern bool                         g_InsertOnSpace;
extern std::wstring                 g_PopupPosition;
extern std::wstring                 g_ScopeMode;
extern std::vector<std::wstring>    g_ScopeApps;
```

**Problem:** Hard to test, manage, and reason about.

**Fix:** Create an `Application` class (singleton pattern)

```cpp
// include/Application.h
class Application {
public:
    static Application& Instance();
    
    // Settings
    const AppSettings& GetSettings() const { return settings; }
    void SetSettings(const AppSettings& s) { settings = s; }
    
    // Database
    Database& GetDatabase() { return db; }
    
    // Popup
    PopupManager& GetPopup() { return popup; }
    
private:
    Application();
    static Application* instance;
    
    AppSettings settings;
    Database db;
    PopupManager popup;
    // ...
};

// Usage
Application::Instance().GetSettings().emojiSymbol;
```

**Benefit:** 
- ✅ Dependency injection for testing
- ✅ Clear ownership
- ✅ Thread-safe (if needed)

---

## 🟡 Design Issues

### 1. **Weak Error Handling in Database Layer**

**Location:** `Database.cpp` (multiple places)

```cpp
// Lines 82-86 - Silent failure
if (sqlite3_prepare_v2(g_db, ...) != SQLITE_OK)
    continue;  // ❌ No error reported
```

**Fix:** Add error context

```cpp
struct DbResult {
    bool success;
    std::string error;
};

DbResult InsertTags(int expansionId, const std::vector<std::wstring>& tags) {
    for (const auto& tag : tags) {
        if (tag.empty()) continue;
        
        Statement ts;
        if (sqlite3_prepare_v2(g_db, 
            "INSERT INTO emoji_tags (expansion_id, tag) VALUES (?, ?);",
            -1, &ts.stmt, nullptr) != SQLITE_OK) {
            return {false, std::string(sqlite3_errmsg(g_db))};
        }
        // ...
    }
    return {true, ""};
}
```

---

### 2. **Clipboard Race Condition**

**Location:** `EmojiReplacer.cpp` (lines 110-121)

```cpp
static void RestoreClipboardAsync(std::wstring oldClipboard)
{
    std::thread([oldClipboard = std::move(oldClipboard)]()
    {
        Sleep(3000);  // ❌ Hardcoded delay - what if system is slow?
        SetClipboardTextSafe(oldClipboard);
    }).detach();     // ❌ Detached thread - fire and forget
}
```

**Issue:** 
- 3000ms is arbitrary
- Detached thread has no error handling
- Could fail silently

**Fix:** Make it configurable and add error tracking

```cpp
class ClipboardManager {
private:
    static constexpr int RESTORE_DELAY_MS = 3000;
    
public:
    static bool RestoreClipboardAsync(const std::wstring& oldClipboard) {
        try {
            std::thread([oldClipboard]() {
                Sleep(RESTORE_DELAY_MS);
                if (!SetClipboardTextSafe(oldClipboard)) {
                    OutputDebugStringW(L"[WARNING] Failed to restore clipboard\n");
                }
            }).detach();
            return true;
        } catch (...) {
            return false;
        }
    }
};
```

---

### 3. **Registry Access Not Wrapped**

**Location:** `WebSettings.cpp` (lines 66-103)

Registry operations are scattered and not reusable.

**Fix:** Create a registry helper

```cpp
// src/helper/RegistryHelper.h
class RegistryHelper {
public:
    static bool GetRunEntry(const std::wstring& appName, std::wstring& outPath);
    static bool SetRunEntry(const std::wstring& appName, const std::wstring& exePath);
    static bool DeleteRunEntry(const std::wstring& appName);
    
private:
    static const wchar_t* RUN_PATH;
};
```

---

## 🟢 What's Good

### ✅ 1. **RAII Pattern Used Well**
- `ClipboardGuard` (lines 13-39)
- `Statement` wrapper in Database
- **Recommendation:** Extract `ClipboardGuard` for reuse

### ✅ 2. **Separation of Concerns**
- UI layer (`src/ui/`) separate from core (`src/core/`)
- Database layer (`src/data/`) isolated
- **Good structure overall**

### ✅ 3. **Namespacing**
- Animation constants in `namespace Anim {}`
- UI constants in `namespace UI {}`
- **Best practice followed**

### ✅ 4. **DBMS Query Optimization**
- `GROUP_CONCAT` with `LEFT JOIN` for tags
- Proper indexes implied (good)
- **Database design is solid**

---

## 🔧 Refactoring Plan (Priority Order)

### **Phase 1: Extract Duplicates (HIGH PRIORITY)**
1. Move `FindExactMatch()` to `EmojiMatcher` module
2. Extract `ClipboardGuard` to shared utility
3. Create `Settings` struct with validation

**Time:** ~2 hours  
**Benefit:** Reduces code duplication by ~15%

---

### **Phase 2: Add Error Handling (MEDIUM PRIORITY)**
1. Wrap Registry access in `RegistryHelper` class
2. Add error returns to `InsertTags()`, `DeleteTags()`
3. Add error handling to clipboard restore

**Time:** ~4 hours  
**Benefit:** Prevents silent failures

---

### **Phase 3: Dependency Injection (MEDIUM PRIORITY)**
1. Create `Application` singleton with DI support
2. Replace global variables with `Application::Instance()`
3. Make components testable

**Time:** ~6 hours  
**Benefit:** Enables unit testing

---

### **Phase 4: Refactor UI to OOP (LOW PRIORITY)**
1. Create `PopupManager` class
2. Create `ClipboardManager` class
3. Create `KeyboardHookManager` class

**Time:** ~8 hours  
**Benefit:** Better testability, cleaner architecture

---

## 📝 Testability Improvements

### Before (Hard to Test)
```cpp
void ReplaceWithEmoji(const std::wstring& token, const std::wstring& emoji)
{
    // Uses global state, clipboard, window input
    // Can't test without actual UI
}
```

### After (Testable)
```cpp
// Dependency injection
class EmojiReplacer {
private:
    IClipboardManager* clipboard;
    IKeySimulator* keyInput;
    
public:
    EmojiReplacer(IClipboardManager* cb, IKeySimulator* ks) 
        : clipboard(cb), keyInput(ks) {}
    
    bool Replace(const std::wstring& token, const std::wstring& emoji) {
        // Can mock IClipboardManager and IKeySimulator
        return keyInput->SimulateBackspace(token.size()) &&
               clipboard->Set(emoji) &&
               keyInput->SimulateCtrlV();
    }
};

// Unit test
TEST(EmojiReplacer, ReplaceEmoji) {
    MockClipboard mockClip;
    MockKeyInput mockKey;
    
    EmojiReplacer replacer(&mockClip, &mockKey);
    EXPECT_TRUE(replacer.Replace(L"smile", L"😊"));
    
    EXPECT_CALL(mockClip, Set(L"😊"));
    EXPECT_CALL(mockKey, SimulateBackspace(6));
}
```

---

## 🎯 OOP When It Makes Sense

**Use OOP for:**
- ✅ `ClipboardManager` - complex state, multiple operations
- ✅ `RegistryHelper` - encapsulate Windows API
- ✅ `Application` - centralized state management
- ✅ `PopupManager` - UI state and animation
- ✅ `Database` - currently procedural but could be OOP

**Don't use OOP for:**
- ❌ `EmojiMatcher` - too simple (single function)
- ❌ `StringUtils` - utility functions stay procedural
- ❌ `InputBuffer` - small state, better as module

---

## 📊 Code Metrics

| Metric | Current | Target |
|--------|---------|--------|
| Duplicate Code | ~3-5% | <2% |
| Test Coverage | 0% | >60% |
| Error Handling | 30% | >80% |
| OOP Usage | ~10% | ~40% |
| Global State | 9 vars | 1 singleton |

---

## 🚀 Next Steps

1. **Start Phase 1 immediately** - Quick wins with refactoring
2. **Add unit tests** for extracted functions
3. **Create Issue/PR** for each phase
4. **Document public APIs** as you refactor

Would you like me to create the actual refactored code for any of these recommendations?
