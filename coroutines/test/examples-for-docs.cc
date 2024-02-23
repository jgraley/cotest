#include "cotest/cotest.h"

using namespace testing;

// ------------- Getting Started --------------

class MyClass {
   public:
    int Method1( int a ) {
        return a * 3;
    }
    void Method2( int &a ) {
        a *= 3;
    }
    int operator+(int a) {
        return a+100;
    }            
};

COTEST(MyTest, Case1) {
    MyClass my_instance;
    
    LaunchHandle<int> l = LAUNCH( my_instance.Method1(24) );
    /// alternative: auto
    ResultHandle r = WAIT_FOR_RESULT();
    EXPECT_TRUE(r);
    EXPECT_EQ(r(l), 72);
    // alternative EXPECT_EQ( WAIT_FOR_RESULT()(l), 72 );
}

COTEST(MyTest, Case2)
{
    MyClass my_instance;
    int i = 24;

    LaunchHandle<void> l = LAUNCH( my_instance.Method2(i) );
    WAIT_FOR_RESULT();
    EXPECT_EQ( i, 72 );
}

COTEST(MyTest, Case3)
{
    MyClass my_instance;

    auto l = LAUNCH( my_instance + 9 );
    EXPECT_EQ( WAIT_FOR_RESULT()(l), 109 );
}

// -------------- Mocking ---------------

class Turtle {
   public:   
    virtual ~Turtle() = default;
    virtual void PenUp() = 0;
    virtual void PenDown() = 0;
    virtual void Forward(int distance) = 0;
    virtual void Turn(int degrees) = 0;
    virtual void GoTo(int x, int y) = 0;
    virtual int GetX() const = 0;
    virtual int GetY() const = 0;
    virtual void InkCheck() const = 0;
};

class MockTurtle : public Turtle {
   public:
    virtual ~MockTurtle() = default;
    MOCK_METHOD(void, PenUp, (), (override));
    MOCK_METHOD(void, PenDown, (), (override));
    MOCK_METHOD(void, Forward, (int distance), (override));
    MOCK_METHOD(void, Turn, (int degrees), (override));
    MOCK_METHOD(void, GoTo, (int x, int y), (override));
    MOCK_METHOD(int, GetX, (), (const, override));
    MOCK_METHOD(int, GetY, (), (const, override));
    MOCK_METHOD(void, InkCheck, (), (const, override));
};

class Painter {
public:
    Painter(Turtle *turtle) : my_turtle(turtle) {}

    void EmptyMethod() {}

    void DrawDot() {
        my_turtle->PenDown();
        my_turtle->PenUp();
    }

    void DrawSquare(int size) {
        my_turtle->PenDown();
        for( int i=0; i<4; i++ ) {
            my_turtle->Forward(size);
            my_turtle->Turn(90);
        }
        my_turtle->PenUp();
    }

    void DrawSquareInkChecks(int size) {
        my_turtle->PenDown();
        for( int i=0; i<4; i++ ) {
            my_turtle->Forward(size);
            my_turtle->Turn(90);
            if( (i % 2) == 0 )
                my_turtle->InkCheck();
        }
        my_turtle->PenUp();
    }

    void CheckPosition() {
        if( my_turtle->GetX() < -100 ||
            my_turtle->GetX() > 100 ||
            my_turtle->GetY() < -100 ||
            my_turtle->GetY() > 100 )
            my_turtle->GoTo(0, 0);
    }
        
    void GoToPointTopLeft() {
        my_turtle->GoTo( -1, 1 );
    }

    void GoToRandomPointOnCircle( int radius ) {
        float a = 2*M_PI * rand() / RAND_MAX;
        my_turtle->GoTo( round(radius*sin(a)),
                         round(radius*cos(a)) );
    }
        
   private:
    Turtle * const my_turtle;
};

COTEST(PainterTest, GoToPointTopLeft)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();
    // Alternatives
    //WATCH_CALL(mock_turtle);
    //WATCH_CALL(mock_turtle, GoTo);

    auto l = LAUNCH( painter.GoToPointTopLeft() );

    MockCallHandle c = WAIT_FOR_CALL();
    EXPECT_TRUE( c.IS_CALL(mock_turtle, GoTo).With(Lt()) );
    c.RETURN();
    WAIT_FOR_RESULT();
}

COTEST(PainterTest, GoToPointTopLeft2)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL(mock_turtle, GoTo).With(Lt());

    auto l = LAUNCH( painter.GoToPointTopLeft() );

    WAIT_FOR_CALL().RETURN();
    WAIT_FOR_RESULT();
    SATISFY(); // Workaround issue #11
}

COTEST(PainterTest, Dot)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l = LAUNCH( painter.DrawDot() );

    SignatureHandle<void()> c = WAIT_FOR_CALL(mock_turtle, PenDown);
    c.RETURN();
    // alternative: WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT();
}


COTEST(PainterTest, CheckPosition)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l = LAUNCH( painter.CheckPosition() );
    WAIT_FOR_CALL(mock_turtle, GetX).RETURN(-200);    
    WAIT_FOR_CALL(mock_turtle, GoTo(0, 0)).RETURN();
    WAIT_FOR_RESULT();        

    // ...

    l = LAUNCH( painter.CheckPosition() );
    WAIT_FOR_CALL(mock_turtle, GetX).RETURN(20);    
    WAIT_FOR_CALL(mock_turtle, GetX).RETURN(20);    
    WAIT_FOR_CALL(mock_turtle, GetY).RETURN(10);    
    WAIT_FOR_CALL(mock_turtle, GetY).RETURN(10);    
    WAIT_FOR_RESULT();        
}

COTEST(PainterTest, Square)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l = LAUNCH( painter.DrawSquare(5) );
    WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    for( int i=0; i<4; i++ )
    {
        WAIT_FOR_CALL(mock_turtle, Forward(5)).RETURN();
        WAIT_FOR_CALL(mock_turtle, Turn(90)).RETURN();
    }
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT();
}

COTEST(PainterTest, SquareFlexibleCase)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l = LAUNCH( painter.DrawSquare(5) );
    WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    
    MockCallHandle mock_call;
    while(true)
    {
        mock_call = WAIT_FOR_CALL(mock_turtle);
        if( !mock_call.IS_CALL(mock_turtle, Forward) )
            break; 
        EXPECT_TRUE( mock_call.IS_CALL(mock_turtle, Forward(5)) );
        mock_call.RETURN();
        WAIT_FOR_CALL(mock_turtle, Turn(90)).RETURN();
    }
    
    EXPECT_TRUE( mock_call.IS_CALL(mock_turtle, PenUp()) );
    mock_call.RETURN();
    WAIT_FOR_RESULT();
}

COTEST(PainterTest, SquareInkChecks1)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();
    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());

    // From here, as before
    auto l = LAUNCH( painter.DrawSquareInkChecks(5) );
    WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    for( int i=0; i<4; i++ )
    {
        WAIT_FOR_CALL(mock_turtle, Forward(5)).RETURN();
        WAIT_FOR_CALL(mock_turtle, Turn(90)).RETURN();
    }
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT();
}

COTEST(PainterTest, SquareInkChecks2)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());
    WATCH_CALL(mock_turtle, PenDown);
    WATCH_CALL(mock_turtle, PenUp);
    WATCH_CALL(mock_turtle, Forward);
    WATCH_CALL(mock_turtle, Turn);

    // From here, as before
    auto l = LAUNCH( painter.DrawSquareInkChecks(5) );
    WAIT_FOR_CALL(mock_turtle, PenDown).RETURN();
    for( int i=0; i<4; i++ )
    {
        WAIT_FOR_CALL(mock_turtle, Forward(5)).RETURN();
        WAIT_FOR_CALL(mock_turtle, Turn(90)).RETURN();
    }
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT();
    SATISFY(); // Workaround issue #11
}



COTEST(PainterTest, RandomPointOnCircle)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l = LAUNCH( painter.GoToRandomPointOnCircle(1000) );
    auto c = WAIT_FOR_CALL(mock_turtle, GoTo);
    float radius_sq = c.GetArg<0>() * c.GetArg<0>() +
                      c.GetArg<1>() * c.GetArg<1>();                            
    EXPECT_NEAR( radius_sq, 1000*1000, 1000 );
    c.RETURN();
    WAIT_FOR_RESULT();
}


COTEST(PainterTest, MultiLaunch)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l1 = LAUNCH( painter.DrawDot() );
    
    auto c1 = WAIT_FOR_CALL_FROM(mock_turtle, PenDown, l1);
    //alternative
    //auto c1 = WAIT_FOR_CALL_FROM(l1);
    //EXPECT_TRUE( c1.IS_CALL(mock_turtle, PenDown).From(l1) );
    // but different dropping rules
    auto l2 = LAUNCH( painter.EmptyMethod() );
    WAIT_FOR_RESULT_FROM(l2);

    c1.RETURN();
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT_FROM(l1);
}


// ------------- Getting Started --------------
 
// For interworking guide, cover COROUTINE() and NEW_COROUTINE(),
// the cardinality API and EXIT_COROUTINE. Do multi-coro examples.

// ------------- Server style --------------

// For serverised guide, cover NEXT_EVENT(), IS_CALL() with no args,
// IS_RESULT() and EventHandle. But it's probably OK to directly reference
// cotest-serverised.cc examples
