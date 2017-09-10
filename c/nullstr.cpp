#include <string>
#include <iostream>

int main(int argc, char **argv) {
    std::string s;
    if (argc > 1)
        s = argv[1];
    else
        s = "";
    std::cout << "My string (empty: " << s.empty() << "): " << s << std::endl;
    return 0;
}
