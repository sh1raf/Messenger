#ifndef DATABASE_HPP
#define DATABASE_HPP

#include <nlohmann/json.hpp>

#include <string>
#include <fstream>
#include <filesystem>
#include <mutex>

#include "../../Containers/hashtable.hpp"
#include "document.hpp"
#include "../../Containers/Go/vector.h"

using nlohmann::json;

mutex mtx;

enum operationState
{
    SUCCESS,
    FAILED
};

class Database 
{
private:
    string dbName;
    string basePath;
    
    string getCollectionPath(const string& collectionName) 
    {
        return basePath + "/" + collectionName + ".json";
    }
    
    void ensureDirectoryExists() 
    {
        filesystem::create_directories(basePath);
    }
    
    string removeQuotes(const string& str) 
    {
        if (str.length() >= 2 && str[0] == '\'' && str[str.length()-1] == '\'') 
        {
            return str.substr(1, str.length()-2);
        }
        return str;
    }

public:
    Database(const string& name) : dbName(name), basePath("databases/" + name) 
    {
        ensureDirectoryExists();
    }
    
    operationState insert(const string& collectionName, const string& documentJson) 
    {
        string cleanJson = removeQuotes(documentJson);

        mtx.lock();
        try 
        {
            json docData = json::parse(cleanJson);
            Document doc(docData);
            
            ChainHashTable<string, Document> collection;
            loadCollection(collectionName, collection);

            collection.insert(doc.getId(), doc);

            saveCollection(collectionName, collection);
            
            cout << "Document inserted successfully." << endl;
            mtx.unlock();
            return operationState::SUCCESS;
        }
        catch (const exception& e) 
        {
            cerr << "Error inserting document: " << e.what() << endl;
            mtx.unlock();
            return operationState::FAILED;
        }
    }
    
    operationState remove(const string& collectionName, const string& queryJson) 
    {
        string cleanJson = removeQuotes(queryJson);
        
        mtx.lock();
        try 
        {
            json query = json::parse(cleanJson);
            ChainHashTable<string, Document> collection;
            loadCollection(collectionName, collection);
            
            myVector<string> idsToRemove;
            auto allDocs = collection.getAll();
            for (size_t i = 0; i < allDocs.size(); i++)
            {
                if (allDocs[i].second.matches(query)) 
                {
                    idsToRemove.push_back(allDocs[i].first);
                }
            }
            
            for (size_t i = 0; i < idsToRemove.size(); i++) 
            {
                collection.remove(idsToRemove[i]);
            }
            
            saveCollection(collectionName, collection);
            cout << "Removed " << idsToRemove.size() << " document(s)." << endl;
            mtx.unlock();
            return operationState::SUCCESS;
        }
        catch (const exception& e) 
        {
            cerr << "Error removing documents: " << e.what() << endl;
            mtx.unlock();
            return operationState::FAILED;
        }
    }
    
    myVector<Document> find(const string& collectionName, const string& queryJson) 
    {
        string cleanJson = removeQuotes(queryJson);

        try 
        {
            json query = json::parse(cleanJson);
            ChainHashTable<string, Document> collection;
            loadCollection(collectionName, collection);
            
            myVector<Document> results;
            auto allDocs = collection.getAll();
            for (size_t i = 0; i < allDocs.size(); i++) 
            {
                if (allDocs[i].second.matches(query)) 
                {
                    results.push_back(allDocs[i].second);
                }
            }
            return results;
        }
        catch (const exception& e) 
        {
            cerr << "Error finding documents: " << e.what() << endl;
        }
    }
    

private:
    void loadCollection(const string& collectionName, ChainHashTable<string, Document>& collection) 
    {
        string filePath = getCollectionPath(collectionName);

        if (!filesystem::exists(filePath)) 
        {
            return;
        }
        
        ifstream file(filePath);
        if (!file.is_open()) 
        {
            throw runtime_error("Cannot open collection file: " + collectionName);
        }
        
        json collectionData;
        file >> collectionData;
        file.close();
        
        for (auto& [key, value] : collectionData.items()) 
        {
            Document doc(value);
            collection.insert(key, doc);
        }
    }
    
    void saveCollection(const string& collectionName, ChainHashTable<string, Document>& collection) 
    {
        string filePath = getCollectionPath(collectionName);
        
        json collectionData;
        auto allDocs = collection.getAll();

        for (size_t i = 0; i < allDocs.size(); i++)
        {
            collectionData[allDocs[i].first] = allDocs[i].second.getData();
        }
        
        ofstream file(filePath);
        file << collectionData.dump(2);
        file.close();
    }
};

#endif