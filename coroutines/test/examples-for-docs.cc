#include "cotest/cotest.h"

using namespace testing;

class MyClass {
public:
    int Method1( int a )
    {
        return a * 3;
    }
};

COTEST(MyTestSuite, TestCase1)
{
    MyClass my_instance;
    
    auto l = LAUNCH( my_instance.Method1(24) );
    EXPECT_EQ( WAIT_FOR_RESULT()(l), 72 );
}
