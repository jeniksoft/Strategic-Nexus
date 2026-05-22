#include "JsonSanity.h"

#include <cctype>
#include <vector>

namespace strategic_nexus::common {

bool hasBalancedJsonDelimiters(const std::string& json)
{
    std::vector<char> stack;
    bool inString = false;
    bool escaped = false;
    bool sawNonWhitespace = false;

    for (const char character : json) {
        if (!sawNonWhitespace && std::isspace(static_cast<unsigned char>(character))) {
            continue;
        }
        sawNonWhitespace = true;

        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (character == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }

        if (character == '{' || character == '[') {
            stack.push_back(character);
            continue;
        }
        if (character == '}') {
            if (stack.empty() || stack.back() != '{') {
                return false;
            }
            stack.pop_back();
            continue;
        }
        if (character == ']') {
            if (stack.empty() || stack.back() != '[') {
                return false;
            }
            stack.pop_back();
            continue;
        }
    }

    return sawNonWhitespace && !inString && !escaped && stack.empty();
}

} // namespace strategic_nexus::common
