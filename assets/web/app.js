﻿// =====================================================
// ===== STATE =====
// =====================================================
let expansions      = [];
let filteredExpansions = [];
let editingId       = null;
let deleteId        = null;
let searchTimeout   = null;
let currentSection  = 'text';   // 'text' | 'emoji'
let currentQuery    = '';

// =====================================================
// ===== CONSTANTS =====
// =====================================================
const DEFAULT_SETTINGS = {
    autoStart:     false,
    emojiSymbol:   ';',
    maxPopup:      7,
    insertTrigger: 'space',
    popupPosition: 'fixed', // 'cursor' | 'fixed'
    scopeMode:     'block',  // 'allow' | 'block'
    scopeApps:     [],       // array of app ids
};
let settings = { ...DEFAULT_SETTINGS };


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
    setupEventListeners();
    syncSettingsUI();
    updateAppScopeSummary();
    postNative('getAll');
});

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

    // ----- Section Tabs -----
    document.querySelectorAll('.tab').forEach(t => {
        t.addEventListener('click', () => switchSection(t.dataset.section));
    });

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
    currentSection = section;

    document.querySelectorAll('.tab').forEach(t => {
        const active = t.dataset.section === section;
        t.classList.toggle('active', active);
        t.setAttribute('aria-selected', String(active));
    });

    document.getElementById('insertFormTitle').textContent =
        section === 'emoji' ? 'Add New Emoji Expansion' : 'Add New Text Expansion';
    document.getElementById('expansionInput').placeholder =
        section === 'emoji' ? 'Emoji only, e.g. 😀' : 'Expanded text...';

    applyFilter();
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
        tableBody.innerHTML = `
            <div class="empty-state">
                <div class="empty-icon">${isEmoji ? '😶' : '📭'}</div>
                <div class="empty-title">No ${isEmoji ? 'Emoji' : 'Text'} Expansions Found</div>
                <div class="empty-text">${
                    currentQuery
                        ? 'Try a different search.'
                        : 'Create your first ' + (isEmoji ? 'emoji' : 'text') + ' expansion below.'
                }</div>
            </div>
        `;
        return;
    }

    tableBody.innerHTML = filteredExpansions.map(item => `
        <div class="table-row" data-id="${item.id}">
            <div class="table-cell-token">${escapeHtml(item.token)}</div>
            <div class="table-cell-expansion">${escapeHtml(item.expansion)}</div>
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
    renderChipPreview('editTagsInput', 'editTagsPreview');
    document.getElementById('editModal').classList.add('active');
}

function closeEditModal() {
    editingId = null;
    document.getElementById('editModal').classList.remove('active');
}

function saveEdit() {
    const token = normalizeToken(document.getElementById('editTokenInput').value);
    const value = document.getElementById('editExpansionInput').value.trim();
    const tags  = parseTags(document.getElementById('editTagsInput').value);

    let ok = true;

    // Validate token, passing the current ID so duplicate check skips itself
    const tokenErr = validateToken(token, editingId);
    showError('editTokenInput', 'editTokenError', tokenErr || '');
    if (tokenErr) ok = false;

    if (!value) {
        showError('editExpansionInput', 'editExpansionError', 'Expansion is required');
        ok = false;
    } else {
        showError('editExpansionInput', 'editExpansionError', '');
    }

    if (!ok) return;

    const type = classify(value);
    postNative(`update|${editingId}|${token}|${value}|${tags.join(',')}|${type}`);
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