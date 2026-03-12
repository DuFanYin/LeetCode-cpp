#include <iostream>
#include <string>

#include "../includes/hash_table.h"

int main() {
    HashTable<std::string, int> ht;

    ht.insert_or_assign("alice", 1);
    ht.insert_or_assign("bob", 2);
    ht.insert_or_assign("alice", 3);  // update

    if (auto* v = ht.find("alice")) {
        std::cout << "alice=" << *v << "\n";
    }

    ht.erase("bob");
    std::cout << "size=" << ht.size() << "\n";
}

