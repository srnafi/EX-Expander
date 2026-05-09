#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"

// ---------------------------------------------------------------------------
//  Expansion record
// ---------------------------------------------------------------------------

struct Expansion
{
    int                       id = 0;
    std::wstring              token;
    std::wstring              value;
    std::wstring              type;       // "emoji" | "text"
    std::vector<std::wstring> tags;       // optional search tags
};

// ---------------------------------------------------------------------------
//  Core CRUD  –  public C-style API (ABI-stable, easy to call from anywhere)
// ---------------------------------------------------------------------------

bool                   DB_Open();
void                   DB_Close();

bool DB_AddExpansion(const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags = {});

bool DB_UpdateExpansion(int id,
    const std::wstring& token,
    const std::wstring& value,
    const std::wstring& type,
    const std::vector<std::wstring>& tags = {});

bool                   DB_DeleteExpansion(int id);
std::vector<Expansion> DB_GetAllExpansions();
std::vector<Expansion> DB_Search(const std::wstring& query);

// ---------------------------------------------------------------------------
//  Seeding helpers
// ---------------------------------------------------------------------------

void DB_Seed();
bool DB_IsEmpty();
void SeedFromJson(const std::wstring& path);

// ---------------------------------------------------------------------------
//  Key-value settings store
// ---------------------------------------------------------------------------

std::wstring DB_GetSetting(const std::wstring& key,
    const std::wstring& defaultVal = L"");
bool         DB_SetSetting(const std::wstring& key,
    const std::wstring& value);