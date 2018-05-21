#include <map>
#include <cstdio>
#include <string>

class Test
{
public:
    int i;
};

static void default_values()
{
    std::map<int, int> m;
    printf("Default map value for an int: %d\n", m[3]);
    std::map<int, std::string> s;
    printf("Default map value for a std::string: %s\n", s[3].c_str());
    std::map<int, Test> t;
    printf("Default map value for a class's int member: %d\n", t[3].i);
    std::map<int, Test*> p;
    printf("Default map value for a class pointer: %p\n", p[3]);
}

static void map_map_contains()
{
    std::map<int, std::map<int, std::string>> map_map;
    map_map[1][0] = "Hello world!";

    const auto& map0 = map_map.find(0);
    if (map0 == map_map.end())
        printf("map_map.find(0) failed (expected)\n");
    else
        printf("map_map.find(0) succeeded (not expected)\n");

    const auto& map1 = map_map.find(1);
    if (map1 != map_map.end()) {
        printf("map_map.find(1) succeeded (expected)\n");
        const auto& submap = map1->second;
        const auto& it = submap.find(0);
        if (it != submap.end()) {
            printf("submap.find(0) succeeded (expected)\n");
            printf("submap[0] == %s\n", it->second.c_str());
        } else
            printf("submap.find(0) failed (not expected)\n");
    } else
        printf("map_map.find(1) failed (not expected)\n");
    printf("Accessed map_map[1][0]: %s\n", map_map[1][0].c_str());
    printf("Accessed map_map[3][3]: %s\n", map_map[3][3].c_str());
}

int main(int argc, char *argv[])
{
    default_values();
    printf("\n");
    map_map_contains();
    printf("\n");

    return 0;
}
