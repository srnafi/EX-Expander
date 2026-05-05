#include "EmojiMatcher.h"
#include "Database.h"

std::vector<Expansion> GetMatches(const std::wstring& token)
{
    if (token.empty())
        return {};

    return DB_Search(token);
}