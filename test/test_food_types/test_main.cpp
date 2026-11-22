#include <unity.h>

#include "FoodTypes.h"

using namespace food;

void setUp(){}
void tearDown(){}

void test_type_index(){
    TEST_ASSERT_EQUAL(0, type_index("beef"));
    TEST_ASSERT_EQUAL(3, type_index("poultry"));
    TEST_ASSERT_EQUAL(8, type_index("other"));
    TEST_ASSERT_EQUAL(-1, type_index(""));
    TEST_ASSERT_EQUAL(-1, type_index("Beef"));
    TEST_ASSERT_EQUAL(-1, type_index(nullptr));
}

void test_type_valid(){
    TEST_ASSERT_TRUE(type_valid(""));
    TEST_ASSERT_TRUE(type_valid("pit"));
    TEST_ASSERT_FALSE(type_valid("chicken"));
    TEST_ASSERT_FALSE(type_valid(nullptr));
}

void test_icons(){
    TEST_ASSERT_NULL(icon_16(-1));
    TEST_ASSERT_NULL(icon_10(FOOD_TYPE_COUNT));
    // beef starts with its horns: top row X..............X
    TEST_ASSERT_EQUAL_HEX8(0x01, icon_16(0)[0]);
    TEST_ASSERT_EQUAL_HEX8(0x80, icon_16(0)[1]);
    // every icon has pixels, and none outside the 10 columns of the small ones
    for(int i = 0; i < FOOD_TYPE_COUNT; i++){
        int set = 0;
        for(int b = 0; b < 32; b++){ set += icon_16(i)[b] != 0; }
        TEST_ASSERT_TRUE(set > 0);
        for(int row = 0; row < 10; row++){ TEST_ASSERT_EQUAL_HEX8(0, icon_10(i)[row * 2 + 1] & 0xfc); }
    }
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_type_index);
    RUN_TEST(test_type_valid);
    RUN_TEST(test_icons);
    return UNITY_END();
}
