#include <map>
#include <stdio.h>

int main(int argc, char *argv[])
{
    std::map<int, int> m;
    printf("Default value for an int in std::map: %d\n", m[3]);

    return 0;
}
