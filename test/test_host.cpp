#include <unity.h>

void setUp(void) {
}

void test_add(void) {
    TEST_ASSERT_EQUAL(3, 2+1);
}

int main(){
    UNITY_BEGIN();

    RUN_TEST(test_add);
    UNITY_END(); // stop unit testing
}