#include <iostream>
#include "postgresql.hpp"

int main()
{
    PostgresDatabase db("host=localhost port=5432 dbname=mes_db user=shirkinson password=mirkill200853");
    db.testConnection();
    return 0;
}