#ifndef QUERY_EVALUATOR_HPP
#define QUERY_EVALUATOR_HPP

#include <nlohmann/json.hpp>

#include <string>
#include <stack>
#include <fstream>
#include <memory>
#include <ctime>
#include <vector>

#include "../../Containers/Stack.h"

using namespace std;
using nlohmann::json;

class QueryEvaluator {
public:

    bool evaluate(const json& doc, const json& query)
    {
        if (query.is_object() && query.contains("$or"))
        {
            return evaluateOr(doc, query["$or"]);
        }
        
        if(query.is_object()) 
        {
            for (auto it = query.begin(); it != query.end(); ++it)
            {
                if (!evaluateField(doc, it.key(), it.value()))
                {
                    return false;
                }
            }
            
            return true;
        }
        
        return false;
    }

private:

    bool evaluateOr(const json& doc, const json& conditions)
    {
        for (const auto& condition : conditions) 
        {
            if (evaluate(doc, condition))
            {
                return true;
            }
        }
        return false;
    }
    
    bool evaluateField(const json& doc, const string& field, const json& condition)
    {
        if (!doc.contains(field))
            return false;
        
        const json& fieldValue = doc[field];
        
        if (condition.is_object())
        {
            for (auto it = condition.begin(); it != condition.end(); ++it)
            {
                string op = it.key();
                const json& opValue = it.value();
                
                if (op == "$eq") 
                {
                    if (fieldValue != opValue)
                    {
                        return false;
                    }
                }
                else if (op == "$gt") 
                {
                    if (fieldValue <= opValue)
                    {
                        return false;
                    }
                }
                else if (op == "$lt") 
                {
                    if (fieldValue >= opValue)
                    {
                        return false;
                    } 
                }
                else if (op == "$like") 
                {
                    if (!wildcardMatchSec(fieldValue.get<string>(), opValue.get<string>())) 
                    {
                        return false;
                    }
                }
                else if (op == "$in") 
                {
                    if (!evaluateIn(fieldValue, opValue))
                    {
                        return false;
                    }
                }
                else
                {
                    return false;
                }
            }
        } 
        else 
        {
            return fieldValue == condition;
        }
    }

    bool evaluateIn(const json& value, const json& array) 
    {
        for (const auto& item : array) 
        {
            if (value == item)
            {
                return true;
            }
        }

        return false;
    }

    bool wildcardMatchSec(const string& text, const string& pattern)
    {
        if (pattern.empty()) {
            return text.empty();
        }
        
        vector<string> patternParts;
        string currentPart;
        
        for (size_t j = 0; j < pattern.length(); j++)
        {
            if (pattern[j] == '%' || pattern[j] == '_')
            {
                if (!currentPart.empty())
                {
                    patternParts.push_back(currentPart);
                    currentPart.clear();
                }
                patternParts.push_back(string(1, pattern[j]));
            }
            else
            {
                currentPart += pattern[j];
            }
        }
        
        if (!currentPart.empty())
        {
            patternParts.push_back(currentPart);
        }
        
        size_t i = 0;
        size_t j = 0;
        size_t star_i = string::npos;
        size_t star_j = string::npos;
        
        while (i < text.length())
        {
            if (j < patternParts.size())
            {
                const string& pat = patternParts[j];
                
                if (pat == "_")
                {
                    i++;
                    j++;
                }
                else if (pat == "%")
                {
                    star_i = i;
                    star_j = j;
                    j++;
                }
                else
                {
                    if (i + pat.length() <= text.length() && 
                        text.compare(i, pat.length(), pat) == 0)
                    {
                        i += pat.length();
                        j++;
                    }
                    else if (star_i != string::npos)
                    {
                        star_i++;
                        i = star_i;
                        j = star_j + 1;
                    }
                    else
                    {
                        return false;
                    }
                }
            }
            else if (star_i != string::npos)
            {
                star_i++;
                i = star_i;
                j = star_j + 1;
            }
            else
            {
                return false;
            }
        }
        
        while (j < patternParts.size() && patternParts[j] == "%")
        {
            j++;
        }
        
        return j >= patternParts.size();
    }

    bool wildcardMatch(const string& text, const string& pattern)
    {
        size_t textLen = text.length();
        size_t patternLen = pattern.length();
        
        size_t i = 0;
        size_t j = 0;
        size_t textStarPos = 0;
        size_t patternStarPos = string::npos;
        
        while (i < textLen)
        {
            if (j < patternLen && (pattern[j] == text[i] || pattern[j] == '_'))
            {
                i++;
                j++;
            }
            else if (j < patternLen && pattern[j] == '%')
            {
                patternStarPos = j;
                textStarPos = i;
                j++;
            }
            else if (patternStarPos != string::npos)
            {
                j = patternStarPos + 1;
                i = ++textStarPos;
            }
            else
            {
                return false;
            }
        }
        
        while (j < patternLen && pattern[j] == '%')
        {
            j++;
        }
        
        return j == patternLen;
    }
};

#endif