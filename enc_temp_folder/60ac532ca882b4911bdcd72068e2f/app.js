﻿// =====================================================
// ===== STATE =====
// =====================================================
let expansions      = [];
let filteredExpansions = [];
let editingId       = null;
let deleteId        = null;
let searchTimeout   = null;
let currentSection  = 'emoji';  // 'text' (transient) | 'emoji' | custom id
let currentQuery    = '';

// =====================================================
// ===== CONSTANTS =====
// =====================================================
const DEFAULT_SETTINGS = {
    autoStart:     false,
    emojiSymbol:   ';',
    maxPopup:      7,
    insertTrigger: 'space',
    scopeMode:     'block',  // 'allow' | 'block'
    scopeApps:     [],       // array of app ids
};
let settings = { ...DEFAULT_SETTINGS };

// User-defined custom types: [{ id, name, color }]
const BUILTIN_TYPES = ['text', 'emoji']; // recognized ids; only 'emoji' is always shown
const ALWAYS_SHOWN_TYPES = ['emoji'];
const DESCRIPTION_MAX = 30;
const TYPE_NAME_MAX = 15;
const TYPE_PALETTE = [
    '#0891b2','#7c3aed','#10b981','#f59e0b','#ef4444',
    '#ec4899','#3b82f6','#84cc16','#f97316','#6366f1',
    '#14b8a6','#a855f7'
];
let customTypes = [];

function loadCustomTypes() {
    try {
        const raw = localStorage.getItem('customTypes');
        if (raw) customTypes = JSON.parse(raw) || [];
    } catch (e) {}
}
function persistCustomTypes() {
    try { localStorage.setItem('customTypes', JSON.stringify(customTypes)); } catch (e) {}
    postNative('types|' + JSON.stringify(customTypes));
}
function slugifyType(name) {
    return name.trim().toLowerCase()
        .replace(/[^a-z0-9]+/g, '-')
        .replace(/^-+|-+$/g, '')
        .slice(0, 20) || 'type';
}
function colorForType(id) {
    let h = 0;
    for (let i = 0; i < id.length; i++) h = (h * 31 + id.charCodeAt(i)) >>> 0;
    return TYPE_PALETTE[h % TYPE_PALETTE.length];
}
function findType(id) {
    if (id === 'text' || id === 'emoji') return { id, name: id === 'text' ? 'Text' : 'Emoji', builtin: true };
    return customTypes.find(t => t.id === id) || null;
}
function typeLabel(id) {
    const t = findType(id);
    return t ? t.name : id;
}
// Returns error message or null
function validateNewTypeName(name) {
    name = (name || '').trim();
    if (!name) return 'Name cannot be empty';
    if (name.length > TYPE_NAME_MAX) return `Max ${TYPE_NAME_MAX} characters`;
    const lower = name.toLowerCase();
    if (lower === 'text' || lower === 'emoji') return 'Reserved name';
    if (customTypes.some(t => t.name.toLowerCase() === lower)) return 'Type already exists';
    return null;
}
function createCustomType(name) {
    const err = validateNewTypeName(name);
    if (err) return { error: err };
    name = name.trim();
    let baseId = 'ct_' + slugifyType(name);
    let id = baseId, n = 2;
    while (customTypes.some(t => t.id === id)) id = baseId + '-' + (n++);
    const t = { id, name, color: colorForType(id) };
    customTypes.push(t);
    persistCustomTypes();
    renderTabs();
    return { type: t };
}
function deleteCustomType(id) {
    const idx = customTypes.findIndex(t => t.id === id);
    if (idx < 0) return;
    customTypes.splice(idx, 1);
    persistCustomTypes();
    // Reassign any expansions of that type back to 'text'
    let moved = 0;
    expansions.forEach(e => {
        if (e.type === id) { e.type = 'text'; moved++; postNative(`update|${e.id}|${e.token}|${e.expansion}|${(e.tags||[]).join(',')}|text`); }
    });
    if (currentSection === id) currentSection = 'text';
    renderTabs();
    applyFilter();
}

// Dummy installed-app catalogue. Replace via window.setInstalledApps([...]).
let installedApps = [
    { id: 'chrome',     name: 'Google Chrome',       path: 'C:\\Program Files\\Google\\Chrome\\chrome.exe',           color: '#ea4335' },
    { id: 'firefox',    name: 'Mozilla Firefox',     path: 'C:\\Program Files\\Mozilla Firefox\\firefox.exe',         color: '#ff7139' },
    { id: 'edge',       name: 'Microsoft Edge',      path: 'C:\\Program Files (x86)\\Microsoft\\Edge\\msedge.exe',    color: '#0078d4' },
    { id: 'vscode',     name: 'Visual Studio Code',  path: 'C:\\Users\\You\\AppData\\Local\\Programs\\Code\\Code.exe', color: '#007acc' },
    { id: 'vs',         name: 'Visual Studio 2022',  path: 'C:\\Program Files\\Microsoft Visual Studio\\2022\\devenv.exe', color: '#5c2d91' },
    { id: 'slack',      name: 'Slack',               path: 'C:\\Users\\You\\AppData\\Local\\slack\\slack.exe',         color: '#4a154b' },
    { id: 'discord',    name: 'Discord',             path: 'C:\\Users\\You\\AppData\\Local\\Discord\\Discord.exe',     color: '#5865f2' },
    { id: 'teams',      name: 'Microsoft Teams',     path: 'C:\\Users\\You\\AppData\\Local\\Microsoft\\Teams\\Teams.exe', color: '#6264a7' },
    { id: 'notion',     name: 'Notion',              path: 'C:\\Users\\You\\AppData\\Local\\Programs\\Notion\\Notion.exe', color: '#111111' },
    { id: 'obsidian',   name: 'Obsidian',            path: 'C:\\Users\\You\\AppData\\Local\\Obsidian\\Obsidian.exe',   color: '#7c3aed' },
    { id: 'word',       name: 'Microsoft Word',      path: 'C:\\Program Files\\Microsoft Office\\root\\Office16\\WINWORD.EXE', color: '#2b579a' },
    { id: 'excel',      name: 'Microsoft Excel',     path: 'C:\\Program Files\\Microsoft Office\\root\\Office16\\EXCEL.EXE',   color: '#217346' },
    { id: 'outlook',    name: 'Microsoft Outlook',   path: 'C:\\Program Files\\Microsoft Office\\root\\Office16\\OUTLOOK.EXE', color: '#0072c6' },
    { id: 'spotify',    name: 'Spotify',             path: 'C:\\Users\\You\\AppData\\Roaming\\Spotify\\Spotify.exe',   color: '#1db954' },
    { id: 'steam',      name: 'Steam',               path: 'C:\\Program Files (x86)\\Steam\\steam.exe',                color: '#1b2838' },
    { id: 'figma',      name: 'Figma',               path: 'C:\\Users\\You\\AppData\\Local\\Figma\\Figma.exe',         color: '#a259ff' },
    { id: 'photoshop',  name: 'Adobe Photoshop',     path: 'C:\\Program Files\\Adobe\\Adobe Photoshop\\Photoshop.exe', color: '#001e36' },
    { id: 'terminal',   name: 'Windows Terminal',    path: 'C:\\Program Files\\WindowsApps\\Terminal\\WindowsTerminal.exe', color: '#0c0c0c' },
    { id: 'explorer',   name: 'File Explorer',       path: 'C:\\Windows\\explorer.exe',                                color: '#fbbf24' },
    { id: 'notepad',    name: 'Notepad',             path: 'C:\\Windows\\System32\\notepad.exe',                       color: '#3b82f6' },
];

let appScopeDraft = { mode: 'block', apps: new Set() };
let appScopeQuery = '';

// =====================================================
// ===== EMOJI DETECTION =====
// =====================================================

// "Emoji-only" = string contains at least one emoji codepoint and no letters/digits.
const EMOJI_RE = /\p{Extended_Pictographic}/u;
const ALNUM_RE = /[\p{L}\p{N}]/u;

function isEmojiOnly(str) {
    if (!str) return false;
    return EMOJI_RE.test(str) && !ALNUM_RE.test(str);
}

function classify(expansion) {
    return isEmojiOnly(expansion) ? 'emoji' : 'text';
}

// =====================================================
// ===== NORMALIZE =====
// =====================================================
function normalizeToken(token) {
    return token.trim().toLowerCase();
}

// =====================================================
// ===== VALIDATION =====
// =====================================================

// Returns an error string on failure, null on success.
function validateToken(token, currentId = null) {
    token = normalizeToken(token);

    if (!token)         return 'Token cannot be empty';
    if (token.length > 50) return 'Max 50 characters';
    if (/\s/.test(token))  return 'No spaces allowed';
    if (/[:;]/.test(token)) return 'Cannot contain : or ;';

    // Allowed character set: a-z, 0-9, _ - . / \ ( ) [ ]
    if (!/^[a-z0-9_\-./\\()[\]]+$/.test(token))
        return 'Allowed: a-z, 0-9, _ - . / \\ ( ) [ ]';

    const exists = expansions.some(
        e => e.token === token && e.id !== currentId
    );
    if (exists) return 'Token already exists';

    return null;
}

// =====================================================
// ===== NATIVE BRIDGE =====
// =====================================================
function postNative(msg) {
    if (window.chrome && window.chrome.webview) {
        window.chrome.webview.postMessage(msg);
    }
}

// =====================================================
// ===== RECEIVE =====
// =====================================================
window.loadFromNative = function (data) {
    expansions = data.map(e => ({
        id:        e.id,
        token:     e.token,
        expansion: e.value,
        tags:      Array.isArray(e.tags)
                       ? e.tags
                       : (typeof e.tags === 'string' && e.tags
                           ? e.tags.split(',').map(s => s.trim()).filter(Boolean)
                           : []),
        type:      e.type || classify(e.value),
        description: typeof e.description === 'string'
                       ? e.description.slice(0, DESCRIPTION_MAX)
                       : (typeof e.note === 'string' ? e.note.slice(0, DESCRIPTION_MAX) : ''),
    }));
    applyFilter();
};

// =====================================================
// ===== FILTER =====
// =====================================================
function applyFilter() {
    const q = currentQuery.trim().toLowerCase();
    filteredExpansions = expansions.filter(e => {
        if (e.type !== currentSection) return false;
        if (!q) return true;
        return (
            e.token.toLowerCase().includes(q) ||
            e.expansion.toLowerCase().includes(q) ||
            (e.tags || []).some(t => t.toLowerCase().includes(q))
        );
    });
    renderTable();
    if (typeof renderTabs === 'function') renderTabs();
}

// =====================================================
// ===== THEME =====
// =====================================================
function applyTheme(mode) {
    document.documentElement.setAttribute('data-theme', mode);
    try { localStorage.setItem('theme', mode); } catch (e) {}
    const btn = document.getElementById('themeToggle');
    if (btn) btn.setAttribute(
        'aria-label',
        mode === 'dark' ? 'Switch to light theme' : 'Switch to dark theme'
    );
}

function initTheme() {
    let saved = null;
    try { saved = localStorage.getItem('theme'); } catch (e) {}
    const prefersDark =
        window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches;
    applyTheme(saved || (prefersDark ? 'dark' : 'light'));
}

// Apply theme before any rendering
initTheme();
window.setTheme = applyTheme;

// =====================================================
// ===== SETTINGS =====
// =====================================================
function loadSettings() {
    try {
        const raw = localStorage.getItem('settings');
        if (raw) settings = { ...DEFAULT_SETTINGS, ...JSON.parse(raw) };
    } catch (e) {}
}

function persistSettings() {
    try { localStorage.setItem('settings', JSON.stringify(settings)); } catch (e) {}
    postNative('settings|' + JSON.stringify(settings));
}

// Called by the host to push updated settings into the UI.
window.applySettings = function (incoming) {
    settings = { ...DEFAULT_SETTINGS, ...(incoming || {}) };
    syncSettingsUI();
};

function syncSettingsUI() {
    document.getElementById('settingAutoStart').checked = !!settings.autoStart;
    document.getElementById('settingMaxPopup').value    = clampPopup(settings.maxPopup);
    document.querySelectorAll('.segmented').forEach(group => {
        const key = group.dataset.setting;
        group.querySelectorAll('button').forEach(b => {
            b.classList.toggle('active', String(b.dataset.value) === String(settings[key]));
        });
    });
}

function clampPopup(n) {
    n = parseInt(n, 10);
    if (isNaN(n)) return DEFAULT_SETTINGS.maxPopup;
    return Math.max(1, Math.min(10, n));
}

function openSettingsModal() {
    syncSettingsUI();
    document.getElementById('settingsModal').classList.add('active');
}

function closeSettingsModal() {
    document.getElementById('settingsModal').classList.remove('active');
}

function saveSettings() {
    settings.autoStart = document.getElementById('settingAutoStart').checked;
    settings.maxPopup  = clampPopup(document.getElementById('settingMaxPopup').value);
    document.querySelectorAll('#settingsModal .segmented').forEach(group => {
        const active = group.querySelector('button.active');
        if (active) settings[group.dataset.setting] = active.dataset.value;
    });
    persistSettings();
    updateAppScopeSummary();
    closeSettingsModal();
}

window.openSettingsModal  = openSettingsModal;
window.closeSettingsModal = closeSettingsModal;
window.saveSettings       = saveSettings;

// =====================================================
// ===== APP SCOPE =====
// =====================================================

// Host can swap in real installed-app data: window.setInstalledApps([{id,name,path,color}, ...])
window.setInstalledApps = function (list) {
    if (Array.isArray(list)) {
        installedApps = list.map(a => ({
            id:    String(a.id || a.name),
            name:  a.name || a.id,
            path:  a.path || '',
            color: a.color || '#0891b2',
        }));
        if (document.getElementById('appScopeModal').classList.contains('active')) {
            renderAppList();
        }
        updateAppScopeSummary();
    }
};

function updateAppScopeSummary() {
    const el = document.getElementById('appScopeSummary');
    if (!el) return;
    const n = (settings.scopeApps || []).length;
    if (n === 0) {
        el.textContent = settings.scopeMode === 'allow'
            ? 'Allow in selected apps — none chosen, expansions are off.'
            : 'Run in every app (no exclusions).';
    } else {
        el.textContent = settings.scopeMode === 'allow'
            ? `Allow in ${n} selected app${n > 1 ? 's' : ''}.`
            : `Block in ${n} selected app${n > 1 ? 's' : ''}.`;
    }
}

function openAppScopeModal() {
    appScopeDraft = {
        mode: settings.scopeMode || 'block',
        apps: new Set(settings.scopeApps || []),
    };
    appScopeQuery = '';
    document.getElementById('appScopeSearch').value = '';
    syncAppScopeMode();
    renderAppList();
    document.getElementById('appScopeModal').classList.add('active');
}

function closeAppScopeModal() {
    document.getElementById('appScopeModal').classList.remove('active');
}

function saveAppScope() {
    settings.scopeMode = appScopeDraft.mode;
    settings.scopeApps = Array.from(appScopeDraft.apps);
    persistSettings();
    updateAppScopeSummary();
    closeAppScopeModal();
}

function syncAppScopeMode() {
    document.querySelectorAll('#appScopeModal .segmented[data-setting="scopeMode"] button')
        .forEach(b => b.classList.toggle('active', b.dataset.value === appScopeDraft.mode));
}

function renderAppList() {
    const list = document.getElementById('appList');
    const q = appScopeQuery.trim().toLowerCase();
    const items = installedApps.filter(a =>
        !q || a.name.toLowerCase().includes(q) || a.path.toLowerCase().includes(q)
    );

    if (items.length === 0) {
        list.innerHTML = `<div class="app-empty">No applications match "${escapeHtml(appScopeQuery)}".</div>`;
    } else {
        list.innerHTML = items.map(a => {
            const checked  = appScopeDraft.apps.has(a.id);
            const initials = a.name.split(/\s+/).slice(0, 2).map(s => s[0]).join('').toUpperCase();
            return `
                <label class="app-row${checked ? ' selected' : ''}" data-id="${escapeHtml(a.id)}">
                    <span class="app-avatar" style="background:${a.color}">${escapeHtml(initials)}</span>
                    <span class="app-meta">
                        <span class="app-name">${escapeHtml(a.name)}</span>
                        <span class="app-path">${escapeHtml(a.path)}</span>
                    </span>
                    <span class="app-check">
                        <input type="checkbox" ${checked ? 'checked' : ''} />
                        <span class="app-check-box">
                            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">
                                <polyline points="20 6 9 17 4 12"></polyline>
                            </svg>
                        </span>
                    </span>
                </label>
            `;
        }).join('');
    }

    updateAppScopeCount();
}

function updateAppScopeCount() {
    const el = document.getElementById('appScopeCount');
    if (!el) return;
    const n = appScopeDraft.apps.size;
    const total = installedApps.length;
    el.textContent = `${n} of ${total} selected`;
}

window.openAppScopeModal  = openAppScopeModal;
window.closeAppScopeModal = closeAppScopeModal;
window.saveAppScope       = saveAppScope;

// =====================================================
// ===== INIT =====
// =====================================================
document.addEventListener('DOMContentLoaded', () => {
    loadSettings();
    loadCustomTypes();
    renderTabs();
    setupEventListeners();
    setupTypeUI();
    syncSettingsUI();
    updateAppScopeSummary();
    postNative('getAll');
});

// =====================================================
// ===== DYNAMIC TABS (built-in + custom types) =====
// =====================================================
function renderTabs() {
    const list = document.getElementById('tabsList');
    if (!list) return;

    const textCount = expansions.filter(e => e.type === 'text').length;
    const showText = textCount > 0 || currentSection === 'text';

    const all = [
        ...(showText ? [{ id: 'text',  name: 'Text',  color: 'var(--primary)', icon: 'text' }] : []),
        { id: 'emoji', name: 'Emoji', color: 'var(--primary)', icon: 'emoji' },
        ...customTypes.map(t => ({ id: t.id, name: t.name, color: t.color, icon: 'folder' })),
    ];

    list.innerHTML = all.map(t => {
        const count = expansions.filter(e => e.type === t.id).length;
        const active = t.id === currentSection ? ' active' : '';
        const icon = t.icon === 'text'
            ? `<svg class="side-tab-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 7V5a1 1 0 0 1 1-1h14a1 1 0 0 1 1 1v2"/><path d="M9 20h6M12 4v16"/></svg>`
            : t.icon === 'emoji'
            ? `<svg class="side-tab-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><path d="M8 14s1.5 2 4 2 4-2 4-2"/><line x1="9" y1="9" x2="9.01" y2="9"/><line x1="15" y1="9" x2="15.01" y2="9"/></svg>`
            : `<span class="side-tab-dot" style="--side-color:${t.color}" aria-hidden="true"></span>`;
        return `
            <button type="button" class="side-tab${active}" data-section="${escapeHtml(t.id)}" role="tab" aria-selected="${active ? 'true' : 'false'}">
                ${icon}
                <span class="side-tab-name">${escapeHtml(t.name)}</span>
                <span class="side-tab-count">${count}</span>
            </button>
        `;
    }).join('');
}



// =====================================================
// ===== EVENTS =====
// =====================================================
function setupEventListeners() {
    const searchInput  = document.getElementById('searchInput');
    const insertForm   = document.getElementById('insertForm');
    const toggle       = document.getElementById('insertFormToggle');
    const wrapper      = document.getElementById('insertFormWrapper');
    const themeBtn     = document.getElementById('themeToggle');
    const settingsBtn  = document.getElementById('settingsToggle');

    // ----- Search & Insert -----
    searchInput.addEventListener('input', debounce(handleSearch, 250));
    insertForm.addEventListener('submit', handleInsert);
    insertForm.addEventListener('reset', () => {
        setTimeout(() => renderChipPreview('tagsInput', 'tagsPreview'), 0);
    });

    // ----- Theme -----
    if (themeBtn) {
        themeBtn.addEventListener('click', () => {
            const current =
                document.documentElement.getAttribute('data-theme') || 'light';
            applyTheme(current === 'dark' ? 'light' : 'dark');
        });
    }

    // ----- Settings -----
    if (settingsBtn) settingsBtn.addEventListener('click', openSettingsModal);

    // ----- App Scope: open / search / select-all / clear / row toggle / mode toggle -----
    const openScopeBtn = document.getElementById('openAppScopeBtn');
    if (openScopeBtn) openScopeBtn.addEventListener('click', openAppScopeModal);

    const scopeSearch = document.getElementById('appScopeSearch');
    if (scopeSearch) {
        scopeSearch.addEventListener('input', e => {
            appScopeQuery = e.target.value;
            renderAppList();
        });
    }

    const selectAllBtn = document.getElementById('appScopeSelectAll');
    if (selectAllBtn) {
        selectAllBtn.addEventListener('click', () => {
            const q = appScopeQuery.trim().toLowerCase();
            installedApps
                .filter(a => !q || a.name.toLowerCase().includes(q) || a.path.toLowerCase().includes(q))
                .forEach(a => appScopeDraft.apps.add(a.id));
            renderAppList();
        });
    }

    const clearBtn = document.getElementById('appScopeClear');
    if (clearBtn) {
        clearBtn.addEventListener('click', () => {
            appScopeDraft.apps.clear();
            renderAppList();
        });
    }

    const appList = document.getElementById('appList');
    if (appList) {
        appList.addEventListener('click', e => {
            const row = e.target.closest('.app-row');
            if (!row) return;
            e.preventDefault();
            const id = row.dataset.id;
            if (appScopeDraft.apps.has(id)) appScopeDraft.apps.delete(id);
            else appScopeDraft.apps.add(id);
            renderAppList();
        });
    }

    const scopeModeGroup = document.querySelector('#appScopeModal .segmented[data-setting="scopeMode"]');
    if (scopeModeGroup) {
        scopeModeGroup.addEventListener('click', e => {
            const btn = e.target.closest('button[data-value]');
            if (!btn) return;
            appScopeDraft.mode = btn.dataset.value;
            syncAppScopeMode();
        });
    }

    // ----- Section Tabs (delegated for dynamic custom tabs) -----
    const tabsList = document.getElementById('tabsList');
    if (tabsList) {
        tabsList.addEventListener('click', e => {
            const tab = e.target.closest('[data-section]');
            if (tab) switchSection(tab.dataset.section);
        });
    }

    // Add-type button (now in sidebar footer, outside tabsList)
    const addTypeBtn = document.getElementById('addTypeBtn');
    if (addTypeBtn) {
        addTypeBtn.addEventListener('click', () => {
            const name = prompt(`New type name (max ${TYPE_NAME_MAX} chars):`);
            if (name == null) return;
            const res = createCustomType(name);
            if (res.error) { alert(res.error); return; }
            switchSection(res.type.id);
        });
    }

    // ----- Sidebar collapse / expand / resize -----
    setupSidebar();

    // ----- Description char counter (edit modal) -----
    const editDescEl = document.getElementById('editDescriptionInput');
    if (editDescEl) {
        editDescEl.addEventListener('input', () => updateCharCounter('editDescriptionInput', 'editDescriptionCount'));
    }




    // ----- Tags: Enter or comma adds a chip; live chip preview -----
    [['tagsInput', 'tagsPreview'], ['editTagsInput', 'editTagsPreview']].forEach(
        ([inp, prev]) => {
            const el = document.getElementById(inp);
            if (!el) return;
            el.addEventListener('input', () => renderChipPreview(inp, prev));
            el.addEventListener('keydown', e => {
                if (e.key === 'Enter') {
                    e.preventDefault();
                    const v = el.value;
                    if (v && !v.endsWith(',')) el.value = v + ', ';
                    renderChipPreview(inp, prev);
                }
            });
        }
    );

    // ----- Settings: segmented buttons -----
    document.querySelectorAll('.segmented').forEach(group => {
        group.addEventListener('click', e => {
            const btn = e.target.closest('button[data-value]');
            if (!btn) return;
            group.querySelectorAll('button').forEach(b => b.classList.remove('active'));
            btn.classList.add('active');
        });
    });

    // ----- Settings: maxPopup stepper -----
    const stepper = document.getElementById('settingMaxPopup');
    document.getElementById('stepperUp').addEventListener('click', () => {
        stepper.value = clampPopup((parseInt(stepper.value, 10) || 0) + 1);
    });
    document.getElementById('stepperDown').addEventListener('click', () => {
        stepper.value = clampPopup((parseInt(stepper.value, 10) || 0) - 1);
    });
    stepper.addEventListener('change', () => {
        stepper.value = clampPopup(stepper.value);
    });

    // ----- Responsive collapse of insert form -----
    const applyResponsiveCollapse = () => {
        if (window.innerWidth <= 768) {
            wrapper.classList.add('collapsed');
            toggle.setAttribute('aria-expanded', 'false');
        } else {
            wrapper.classList.remove('collapsed');
            toggle.setAttribute('aria-expanded', 'true');
        }
    };
    applyResponsiveCollapse();
    window.addEventListener('resize', applyResponsiveCollapse);

    toggle.addEventListener('click', () => {
        if (window.innerWidth > 768) return;
        const collapsed = wrapper.classList.toggle('collapsed');
        toggle.setAttribute('aria-expanded', String(!collapsed));
    });

    // ----- Input restriction (token fields) -----
    const tokenInput     = document.getElementById('tokenInput');
    const editTokenInput = document.getElementById('editTokenInput');
    [tokenInput, editTokenInput].forEach(el => {
        el.addEventListener('keydown', restrictInput);
    });
}

// =====================================================
// ===== INPUT RESTRICTION =====
// =====================================================

// Blocks disallowed keystrokes in token input fields before they reach the value.
function restrictInput(e) {
    const char = e.key;

    // Always block spaces and the reserved separator characters
    if (char === ' ' || char === ':' || char === ';') {
        e.preventDefault();
        return;
    }

    // Pass through control sequences (Ctrl+C, Backspace, arrow keys, etc.)
    if (e.ctrlKey || e.metaKey || char.length > 1) return;

    // Allow only the permitted character set: a-z A-Z 0-9 _ - . / \ ( ) [ ]
    if (!/[a-zA-Z0-9_\-./\\()[\]]/.test(char)) {
        e.preventDefault();
    }
}

// =====================================================
// ===== SECTION SWITCH =====
// =====================================================
function switchSection(section) {
    if (section === currentSection) return;
    if (!findType(section)) return;
    currentSection = section;
    renderTabs();

    const titleEl = document.getElementById('insertFormTitle');
    if (titleEl) titleEl.textContent = `Add New ${typeLabel(section)} Expansion`;
    const expIn = document.getElementById('expansionInput');
    if (expIn) expIn.placeholder = section === 'emoji' ? 'Emoji only, e.g. 😀' : 'Expanded text...';

    applyFilter();

    // Scroll active sidebar tab into view
    const active = document.querySelector('#tabsList .side-tab.active');
    if (active && active.scrollIntoView) {
        active.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
}



// =====================================================
// ===== TAGS =====
// =====================================================

// Splits a comma-separated string into a deduplicated, trimmed array of tags.
function parseTags(raw) {
    return (raw || '')
        .split(',')
        .map(s => s.trim())
        .filter(Boolean)
        .filter((t, i, arr) =>
            arr.findIndex(x => x.toLowerCase() === t.toLowerCase()) === i
        );
}

function renderChipPreview(inputId, previewId) {
    const tags    = parseTags(document.getElementById(inputId).value);
    const preview = document.getElementById(previewId);
    preview.innerHTML =
        tags.map(t => `<span class="tag-chip">${escapeHtml(t)}</span>`).join('');
}

// =====================================================
// ===== SEARCH =====
// =====================================================
function handleSearch(e) {
    currentQuery = e.target.value;
    applyFilter();
    postNative('search|' + currentQuery + '|' + currentSection);
}

// =====================================================
// ===== DEBOUNCE =====
// =====================================================
function debounce(fn, delay) {
    return function (...args) {
        clearTimeout(searchTimeout);
        searchTimeout = setTimeout(() => fn.apply(this, args), delay);
    };
}

// =====================================================
// ===== TABLE =====
// =====================================================
function renderTable() {
    const tableBody = document.getElementById('tableBody');

    if (filteredExpansions.length === 0) {
        const isEmoji = currentSection === 'emoji';
        const label = typeLabel(currentSection);
        const icon = isEmoji ? '😶' : (currentSection === 'text' ? '📭' : '🗂️');
        tableBody.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">${icon}</div>
                <div class="empty-title">No ${escapeHtml(label)} Expansions Found</div>
                <div class="empty-text">${
                    currentQuery
                        ? 'Try a different search.'
                        : 'Create your first ' + escapeHtml(label.toLowerCase()) + ' expansion with the New Expansion button.'
                }</div>
            </div>
        `;
        return;
    }


    tableBody.innerHTML = filteredExpansions.map(item => `
        <div class="table-row" data-id="${item.id}">
            <div class="table-cell-token">${escapeHtml(item.token)}</div>
            <div class="table-cell-expansion">
                <div class="cell-expansion-text">${escapeHtml(item.expansion)}</div>
                ${item.description
                    ? `<div class="cell-expansion-desc" title="${escapeHtml(item.description)}">${escapeHtml(item.description)}</div>`
                    : ''}
            </div>
            <div class="table-cell-tags">
                ${(item.tags || [])
                    .map(t => `<span class="tag-chip">${escapeHtml(t)}</span>`)
                    .join('')}
            </div>
            <div class="table-actions">
                <button class="btn-icon btn-edit"
                        onclick="openEditModal(${item.id})" title="Edit">
                    <svg viewBox="0 0 24 24" fill="none">
                        <path d="M3 17.25V21h3.75L19.81 7.94l-3.75-3.75L3 17.25Z"
                              stroke="currentColor" stroke-width="2"
                              stroke-linecap="round" stroke-linejoin="round"/>
                    </svg>
                </button>
                <button class="btn-icon btn-danger"
                        onclick="openDeleteModal(${item.id})" title="Delete">
                    <svg viewBox="0 0 24 24" fill="none">
                        <path d="M18 6L6 18M6 6l12 12"
                              stroke="currentColor" stroke-width="2.2"
                              stroke-linecap="round"/>
                    </svg>
                </button>
            </div>
        </div>
    `).join('');
}

// =====================================================
// ===== ERROR =====
// =====================================================

// Attaches or clears an inline validation error on an input field.
function showError(inputId, errorId, msg) {
    const input = document.getElementById(inputId);
    const err   = document.getElementById(errorId);
    if (msg) {
        input.classList.add('error');
        err.textContent = msg;
        err.classList.add('show');
    } else {
        input.classList.remove('error');
        err.classList.remove('show');
    }
}

// =====================================================
// ===== INSERT =====
// =====================================================
function handleInsert(e) {
    e.preventDefault();

    const token = normalizeToken(document.getElementById('tokenInput').value);
    const value = document.getElementById('expansionInput').value.trim();
    const tags  = parseTags(document.getElementById('tagsInput').value);

    let ok = true;

    // Validate token via shared rules (length, chars, duplicates, etc.)
    const tokenErr = validateToken(token);
    showError('tokenInput', 'tokenError', tokenErr || '');
    if (tokenErr) ok = false;

    // Validate expansion value
    if (!value) {
        showError('expansionInput', 'expansionError', 'Expansion is required');
        ok = false;
    } else if (currentSection === 'emoji' && !isEmojiOnly(value)) {
        showError('expansionInput', 'expansionError',
            'Emoji section accepts emoji only — no letters or numbers');
        ok = false;
    } else {
        showError('expansionInput', 'expansionError', '');
    }

    if (!ok) return;

    const type = currentSection;
    postNative(`insert|${token}|${value}|${tags.join(',')}|${type}`);

    document.getElementById('insertForm').reset();
    renderChipPreview('tagsInput', 'tagsPreview');
}

// =====================================================
// ===== EDIT =====
// =====================================================
function openEditModal(id) {
    const item = expansions.find(x => x.id === id);
    if (!item) return;
    editingId = id;

    document.getElementById('editTokenInput').value     = item.token;
    document.getElementById('editExpansionInput').value = item.expansion;
    document.getElementById('editTagsInput').value      = (item.tags || []).join(', ');
    const descEl = document.getElementById('editDescriptionInput');
    if (descEl) {
        descEl.value = item.description || '';
        updateCharCounter('editDescriptionInput', 'editDescriptionCount');
    }
    populateTypeSelect('editTypeSelect', item.type || classify(item.expansion));
    hideTypeCreateRow('edit');
    showError('editTypeSelect', 'editTypeError', '');
    renderChipPreview('editTagsInput', 'editTagsPreview');
    document.getElementById('editModal').classList.add('active');
}

function closeEditModal() {
    editingId = null;
    document.getElementById('editModal').classList.remove('active');
}

function saveEdit() {
    const token = normalizeToken(document.getElementById('editTokenInput').value);
    const value = document.getElementById('editExpansionInput').value;
    const tags  = parseTags(document.getElementById('editTagsInput').value);
    const typeSel = document.getElementById('editTypeSelect').value;
    const descEl = document.getElementById('editDescriptionInput');
    const description = (descEl ? descEl.value : '').trim().slice(0, DESCRIPTION_MAX);

    let ok = true;

    const tokenErr = validateToken(token, editingId);
    showError('editTokenInput', 'editTokenError', tokenErr || '');
    if (tokenErr) ok = false;

    if (!value.trim()) {
        showError('editExpansionInput', 'editExpansionError', 'Expansion is required');
        ok = false;
    } else if (typeSel === 'emoji' && !isEmojiOnly(value.trim())) {
        showError('editExpansionInput', 'editExpansionError',
            'Emoji type accepts emoji only — change content or pick a different type');
        ok = false;
    } else {
        showError('editExpansionInput', 'editExpansionError', '');
    }

    if (!findType(typeSel)) {
        showError('editTypeSelect', 'editTypeError', 'Pick a type');
        ok = false;
    } else {
        showError('editTypeSelect', 'editTypeError', '');
    }

    if (!ok) return;

    postNative(`update|${editingId}|${token}|${value}|${tags.join(',')}|${typeSel}|${description}`);
    // Local update so UI reflects type change immediately if native echo is missing
    const idx = expansions.findIndex(e => e.id === editingId);
    if (idx >= 0) {
        expansions[idx] = { ...expansions[idx], token, expansion: value, tags, type: typeSel, description };
        applyFilter();
    }
    closeEditModal();
}


// =====================================================
// ===== DELETE =====
// =====================================================
function openDeleteModal(id) {
    deleteId = id;
    document.getElementById('deleteModal').classList.add('active');
}

function closeDeleteModal() {
    deleteId = null;
    document.getElementById('deleteModal').classList.remove('active');
}

function confirmDelete() {
    postNative(`delete|${deleteId}`);
    closeDeleteModal();
}

// =====================================================
// ===== UTIL =====
// =====================================================
function escapeHtml(text) {
    const div     = document.createElement('div');
    div.textContent = text == null ? '' : String(text);
    return div.innerHTML;
}

// Updates "N / MAX" indicator next to a text input. Adds .near-limit / .at-limit classes.
function updateCharCounter(inputId, counterId) {
    const input = document.getElementById(inputId);
    const counter = document.getElementById(counterId);
    if (!input || !counter) return;
    const max = parseInt(input.getAttribute('maxlength'), 10) || DESCRIPTION_MAX;
    const len = (input.value || '').length;
    counter.textContent = `${len} / ${max}`;
    counter.classList.toggle('at-limit', len >= max);
    counter.classList.toggle('near-limit', len >= max - 5 && len < max);
}

// =====================================================
// ===== KEYBOARD =====
// =====================================================
document.addEventListener('keydown', e => {
    if (e.key === 'Escape') {
        closeEditModal();
        closeDeleteModal();
        closeSettingsModal();
    }
});

// =====================================================
// ===== EXPORTS =====
// =====================================================

// Expose functions used by inline onclick handlers in the HTML template.
window.openEditModal    = openEditModal;
window.closeEditModal   = closeEditModal;
window.saveEdit         = saveEdit;
window.openDeleteModal  = openDeleteModal;
window.closeDeleteModal = closeDeleteModal;
window.confirmDelete    = confirmDelete;

// =====================================================
// ===== TYPE PICKER (modal selects) =====
// =====================================================
const NEW_TYPE_SENTINEL = '__new_type__';

function populateTypeSelect(selectId, currentType) {
    const sel = document.getElementById(selectId);
    if (!sel) return;
    const opts = [
        { value: 'text',  label: 'Text' },
        { value: 'emoji', label: 'Emoji' },
        ...customTypes.map(t => ({ value: t.id, label: t.name })),
        { value: NEW_TYPE_SENTINEL, label: '+ New type…' },
    ];
    sel.innerHTML = opts.map(o =>
        `<option value="${escapeHtml(o.value)}"${o.value === currentType ? ' selected' : ''}>${escapeHtml(o.label)}</option>`
    ).join('');
}

function hideTypeCreateRow(prefix) {
    const row = document.getElementById(prefix + 'TypeCreateRow');
    const input = document.getElementById(prefix + 'TypeCreateInput');
    if (row) row.hidden = true;
    if (input) input.value = '';
    showError(prefix + 'TypeSelect', prefix + 'TypeError', '');
}

function setupTypeUI() {
    ['nx', 'edit'].forEach(prefix => {
        const sel = document.getElementById(prefix + 'TypeSelect');
        if (!sel) return;
        sel.addEventListener('change', () => {
            const row = document.getElementById(prefix + 'TypeCreateRow');
            if (sel.value === NEW_TYPE_SENTINEL) {
                row.hidden = false;
                document.getElementById(prefix + 'TypeCreateInput').focus();
            } else {
                row.hidden = true;
            }
        });
        const createInput = document.getElementById(prefix + 'TypeCreateInput');
        const handleCreate = () => {
            const name = createInput.value;
            const res = createCustomType(name);
            if (res.error) {
                showError(prefix + 'TypeSelect', prefix + 'TypeError', res.error);
                return;
            }
            populateTypeSelect(prefix + 'TypeSelect', res.type.id);
            hideTypeCreateRow(prefix);
        };
        createInput.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') { e.preventDefault(); handleCreate(); }
        });
        const btn = document.querySelector(`[data-create-type="${prefix}"]`);
        if (btn) btn.addEventListener('click', handleCreate);
    });
}

// =====================================================
// =====================================================
// =========== NEW EXPANSION MODAL (added) =============
// Premium editor flow that replaces the old inline form.
// Plain-text only. Multi-line supported.
// All new code lives below this line — nothing above is
// modified. Bridges into existing validateToken / postNative.
// =====================================================
// =====================================================
(function () {
    // ---------- DOM helpers ----------
    const $ = (id) => document.getElementById(id);

    function nxOpen() {
        // Reset
        $('nxTokenInput').value = '';
        $('nxExpansionInput').value = '';
        $('nxTagsInput').value = '';
        $('nxDescriptionInput').value = '';
        updateCharCounter('nxDescriptionInput', 'nxDescriptionCount');
        // Default the type to whatever section the user is currently viewing
        const initialType = (currentSection === 'text' || currentSection === 'emoji' || findType(currentSection))
            ? currentSection : 'emoji';
        populateTypeSelect('nxTypeSelect', initialType);
        hideTypeCreateRow('nx');
        nxRenderTags();
        nxRenderPreview();
        showError('nxTokenInput', 'nxTokenError', '');
        showError('nxExpansionInput', 'nxExpansionError', '');
        $('newExpansionModal').classList.add('active');
        setTimeout(() => $('nxTokenInput').focus(), 30);
    }


    function nxClose() {
        $('newExpansionModal').classList.remove('active');
    }

    // ---------- Tags ----------
    function nxRenderTags() {
        renderChipPreview('nxTagsInput', 'nxTagsPreview');
    }

    // ---------- Preview ----------
    function nxRenderPreview() {
        const v = $('nxExpansionInput').value;
        const p = $('nxPreview');
        if (!v) {
            p.innerHTML = '<span class="nx-preview-empty">Your expansion will appear here.</span>';
            return;
        }
        p.textContent = v;
    }

    // ---------- Save ----------
    function nxSave() {
        const token = normalizeToken($('nxTokenInput').value);
        const value = $('nxExpansionInput').value;          // preserve newlines
        const tags  = parseTags($('nxTagsInput').value);
        const type  = $('nxTypeSelect').value;
        const description = ($('nxDescriptionInput').value || '').trim().slice(0, DESCRIPTION_MAX);

        let ok = true;
        const tokenErr = validateToken(token);
        showError('nxTokenInput', 'nxTokenError', tokenErr || '');
        if (tokenErr) ok = false;

        if (!findType(type)) {
            showError('nxTypeSelect', 'nxTypeError', 'Pick a type (or create one)');
            ok = false;
        } else {
            showError('nxTypeSelect', 'nxTypeError', '');
        }

        if (!value.trim()) {
            showError('nxExpansionInput', 'nxExpansionError', 'Expansion is required');
            ok = false;
        } else if (type === 'emoji' && !isEmojiOnly(value.trim())) {
            showError('nxExpansionInput', 'nxExpansionError',
                'Emoji type accepts emoji only — change content or pick a different type');
            ok = false;
        } else {
            showError('nxExpansionInput', 'nxExpansionError', '');
        }

        if (!ok) return;

        postNative(`insert|${token}|${value}|${tags.join(',')}|${type}|${description}`);

        // Optimistic local insert so user sees their new expansion immediately
        const tempId = Date.now();
        expansions.push({ id: tempId, token, expansion: value, tags, type, description });
        // Switch view to the type just created in, so they see it
        if (currentSection !== type) {
            currentSection = type;
            renderTabs();
        }
        applyFilter();

        nxClose();
    }

    // ---------- Wiring ----------
    function nxInit() {
        const openBtn = $('openNewExpansionBtn');
        if (!openBtn) return;
        openBtn.addEventListener('click', nxOpen);
        $('nxCloseBtn').addEventListener('click', nxClose);
        $('nxCancelBtn').addEventListener('click', nxClose);
        $('nxSaveBtn').addEventListener('click', nxSave);

        // Backdrop click closes
        $('newExpansionModal').addEventListener('click', (e) => {
            if (e.target.id === 'newExpansionModal') nxClose();
        });

        // Token restriction (reuses existing handler)
        $('nxTokenInput').addEventListener('keydown', restrictInput);

        // Editor input -> preview
        const ta = $('nxExpansionInput');
        ta.addEventListener('input', nxRenderPreview);

        // Description char counter
        const descEl = $('nxDescriptionInput');
        if (descEl) {
            descEl.addEventListener('input', () => updateCharCounter('nxDescriptionInput', 'nxDescriptionCount'));
        }

        // Tags: comma / Enter add chip
        const tagsEl = $('nxTagsInput');
        tagsEl.addEventListener('input', nxRenderTags);
        tagsEl.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') {
                e.preventDefault();
                const v = tagsEl.value;
                if (v && !v.endsWith(',')) tagsEl.value = v + ', ';
                nxRenderTags();
            }
        });

        // Esc closes (in addition to existing global listener)
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') nxClose();
        });
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', nxInit);
    } else {
        nxInit();
    }

    // Expose for console / host integration
    window.openNewExpansionModal  = nxOpen;
    window.closeNewExpansionModal = nxClose;
})();

// =====================================================
// ===== SIDEBAR: collapse + resize =====
// =====================================================
const SIDEBAR_MIN = 180;
const SIDEBAR_MAX = 480;
const SIDEBAR_DEFAULT = 240;

function loadSidebarWidth() {
    let w = SIDEBAR_DEFAULT;
    let collapsed = false;
    try {
        const raw = localStorage.getItem('sidebarWidth');
        if (raw) w = Math.max(SIDEBAR_MIN, Math.min(SIDEBAR_MAX, parseInt(raw, 10) || SIDEBAR_DEFAULT));
        collapsed = localStorage.getItem('sidebarCollapsed') === '1';
    } catch (e) {}
    applySidebarWidth(w);
    setSidebarCollapsed(collapsed, /*persist*/ false);
}

function applySidebarWidth(w) {
    document.documentElement.style.setProperty('--sidebar-w', w + 'px');
}

function setSidebarCollapsed(collapsed, persist = true) {
    const sb = document.getElementById('sidebar');
    const expandBtn = document.getElementById('sidebarExpandBtn');
    if (!sb) return;
    sb.classList.toggle('collapsed', collapsed);
    if (expandBtn) expandBtn.hidden = !collapsed;
    if (persist) {
        try { localStorage.setItem('sidebarCollapsed', collapsed ? '1' : '0'); } catch (e) {}
    }
}

function setupSidebar() {
    loadSidebarWidth();

    const collapseBtn = document.getElementById('sidebarCollapseBtn');
    const expandBtn   = document.getElementById('sidebarExpandBtn');
    const resizer     = document.getElementById('sidebarResizer');

    if (collapseBtn) collapseBtn.addEventListener('click', () => setSidebarCollapsed(true));
    if (expandBtn)   expandBtn.addEventListener('click', () => setSidebarCollapsed(false));

    // ----- Drag resize -----
    if (resizer) {
        let dragging = false;
        let startX = 0;
        let startW = SIDEBAR_DEFAULT;

        const onMove = (e) => {
            if (!dragging) return;
            const x = e.touches ? e.touches[0].clientX : e.clientX;
            const dx = x - startX;
            const next = Math.max(SIDEBAR_MIN, Math.min(SIDEBAR_MAX, startW + dx));
            applySidebarWidth(next);
        };
        const onUp = () => {
            if (!dragging) return;
            dragging = false;
            document.body.classList.remove('resizing-sidebar');
            resizer.classList.remove('dragging');
            window.removeEventListener('mousemove', onMove);
            window.removeEventListener('mouseup', onUp);
            window.removeEventListener('touchmove', onMove);
            window.removeEventListener('touchend', onUp);
            const cur = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--sidebar-w'), 10);
            try { localStorage.setItem('sidebarWidth', String(cur)); } catch (e) {}
        };
        const onDown = (e) => {
            const sb = document.getElementById('sidebar');
            if (!sb || sb.classList.contains('collapsed')) return;
            dragging = true;
            startX = e.touches ? e.touches[0].clientX : e.clientX;
            startW = sb.getBoundingClientRect().width;
            document.body.classList.add('resizing-sidebar');
            resizer.classList.add('dragging');
            window.addEventListener('mousemove', onMove);
            window.addEventListener('mouseup', onUp);
            window.addEventListener('touchmove', onMove, { passive: false });
            window.addEventListener('touchend', onUp);
            e.preventDefault();
        };
        resizer.addEventListener('mousedown', onDown);
        resizer.addEventListener('touchstart', onDown, { passive: false });

        // Keyboard support
        resizer.addEventListener('keydown', (e) => {
            const cur = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--sidebar-w'), 10) || SIDEBAR_DEFAULT;
            const step = e.shiftKey ? 24 : 8;
            if (e.key === 'ArrowLeft') {
                const next = Math.max(SIDEBAR_MIN, cur - step);
                applySidebarWidth(next);
                try { localStorage.setItem('sidebarWidth', String(next)); } catch (err) {}
                e.preventDefault();
            } else if (e.key === 'ArrowRight') {
                const next = Math.min(SIDEBAR_MAX, cur + step);
                applySidebarWidth(next);
                try { localStorage.setItem('sidebarWidth', String(next)); } catch (err) {}
                e.preventDefault();
            }
        });

        // Double-click resets to default
        resizer.addEventListener('dblclick', () => {
            applySidebarWidth(SIDEBAR_DEFAULT);
            try { localStorage.setItem('sidebarWidth', String(SIDEBAR_DEFAULT)); } catch (e) {}
        });
    }
}

