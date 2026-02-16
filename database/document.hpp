#pragma once 
#define DOCUMENT_HPP

#include <nlohmann/json.hpp>

#include <string>
#include <fstream>
#include <memory>
#include <ctime>

#include "QueryEvaluator.hpp"

using nlohmann::json;

class Document
{
private: 
    json data;
    string id;

public:
    Document() 
    {
        id = generateId();
        data["_id"] = id;
    }
    
    Document(const json& jsonData) : data(jsonData) 
    {
        if (!jsonData.contains("_id")) 
        {
            id = generateId();
            data["_id"] = id;
        } 
        else 
        {
            id = jsonData["_id"];
        }
    }
    
    const string& getId() const 
    { 
        return id; 
    }
    const json& getData() const 
    { 
        return data; 
    }
    
    void setField(const string& field, const json& value) 
    {
        data[field] = value;
    }
    
    bool matches(const json& query) const 
    {
        QueryEvaluator evaluator;
        return evaluator.evaluate(data, query);
    }
    
private:
    string generateId() 
    {
        return "doc_" + to_string(time(nullptr));
    }
};