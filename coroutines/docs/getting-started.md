# Getting Started With Cotest

## Launching the code-under-test 

Suppose we wish to test this C++ class:
```
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
```
In most testing frameworks, we would simply call the methods on an instance of the class, and check the return value and/or side effects. 

#### Simple example

In Cotest, this call is replaced by two steps: we _launch_ the code-under-test, and then we _wait_ for the launch to complete.

This is accomplished as follows:
 - To launch, we use `LAUNCH( <expression> )` which returns a _launch handle_.
 - To wait for completion, we use `WAIT_FOR_RESULT()` which returns a _result handle_.

```
COTEST(MyTest, Case1) {
    MyClass my_instance;
    
    LaunchHandle<int> launch_handle = LAUNCH( my_instance.Method1(24) );
    ResultHandle result_handle = WAIT_FOR_RESULT();
    EXPECT_TRUE(result_handle);
    EXPECT_EQ(result_handle(launch_handle), 72);
}
```
Note that:
 1. `LaunchHandle` is templated on the result type, which is the `decltype()` of the supplied expression.
 2. To extract the actual return value, we use function call syntax, combining the two handles: `result(launch)`

#### More compact example

To save typing, we can use `auto` for handles. We can also make the extraction of return value more compact. So we could write simply
```
COTEST(MyTest, Case1) {
    MyClass my_instance;
    
    auto launch_handle = LAUNCH( my_instance.Method1(24) );
    EXPECT_EQ( WAIT_FOR_RESULT()(l), 72 );
}
```
#### Void return and ref argument example

Let's vary things a bit. This time, the function we call
 - will have void return, and
 - will take a reference (and cause a side-effect through it).

```
COTEST(MyTest, Case2)
{
    MyClass my_instance;
    int i = 24;

    auto l = LAUNCH( my_instance.Method2(i) );
    WAIT_FOR_RESULT();
    EXPECT_EQ( i, 72 );
}
```
In the case of void return, Cotest requires that we use `WAIT_FOR_RESULT()` or similar, in order to prove that the test case has checked for completion of the launch. This must happen before the launch handle goes out of scope, even though we don't use it directly.

We can see that the code under test has successfully modfied our local variable `i`. Thus, Cotest respects reference arguments to launches.

#### Arbitrary expression example

The `LAUNCH()` macro takes an expression, and this does not need to be of the form `<object>.<method(<args>)`. For example, we can test an operator using its intended syntax:

```
COTEST(MyTest, Case3)
{
    MyClass my_instance;

    auto l = LAUNCH( my_instance + 9 );
    EXPECT_EQ( WAIT_FOR_RESULT()(l), 109 );
}
```
## Mocking with Cotest

Please see [the test case for the examples](/coroutines/test/examples-for-docs.cc) for code-under-test and mocking assets - this way we can concentrate on the Cotest test cases.

#### Test with a mock call example

Let's call a code-under-test function that makes a mock call. We will 
 - Inject a dependency onto our mock object by passing a pointer to it to the code-under-test.
 - Make sure Cotest can _see_ mock calls using `WATCH_CALL()`
 - The test proceeds as seen above apart from the inclusion of mock handling code.

```
COTEST(PainterTest, GoToPoint)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);

    WATCH_CALL();

    auto l = LAUNCH( painter.GoToPointTopLeft() );

    MockCallHandle call_handle = WAIT_FOR_CALL();
    EXPECT_TRUE( call_handle.IS_CALL(mock_turtle, GoTo(_, 1)) );
    call_handle.RETURN();

    WAIT_FOR_RESULT();
}
```
To handle the mock:
 - We have used `WAIT_FOR_CALL()` which will give us a mock call handle as soon as the mock call is made.
 - We have checked that the call is correct using `IS_CALL()` which has semantics similar to Google Mock's `EXPECT_CALL()`; a match evaluates to `true`.
 - We instruct the mock call to return. `GoTo()` returns void so no value is required. 

> [!TIP]
> Handles are nullable types, which means they have a null value which evaluates to `false` when used as a boolean.
> Valid handles evaluate to `true`.
> `IS_CALL()` and similar functions return the same (valid) handle on match and a null handle on false.
> It is _not_ an error to invoke functions on null handles - they will just return another null handle, introducing an _and_-rule.

In place of `WATCH_CALL()` we could have used:
 - `WATCH_CALL(mock_turtle)` to only see calls to that mock object or
 - `WATCH_CALL(mock_turtle, GoTo)` to only see calls to that method, or for example
 - `WATCH_CALL(mock_turtle, GoTo(_, 1))` to only see acceptible calls.

#### Filtering calls in the watch example

In the case of `WATCH_CALL(mock_turtle, GoTo(_, 1))`, we would not need to check inside the coroutine and could use just
```
COTEST(PainterTest, GoToPoint2)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);

    WATCH_CALL(mock_turtle, GoTo(_,1));

    auto l = LAUNCH( painter.GoToPointTopLeft() );

    WAIT_FOR_CALL().RETURN();

    WAIT_FOR_RESULT();
    SATISFY(); // Workaround issue #11
}
```
The more restrictive forms of `WATCH_CALL()` will prevent the coroutine from "seeing" calls that don't match. These calls will then be dealt with by Google Mock in the same way as a call that has no matching `EXPECT_CALL()`. Indeed, `WATCH_CALL()` is the Cotest counterpart to `EXPECT_CALL()`.

#### Mock return affects behaviour example

To consolidate, let's try using Cotest for a case where the return values we supply from mock calls will affect behaiour of the code-under-test. In this case, the code-under-test stops calling `GetX()` or `GetY()` as soon as one of them returns a co-ordinate that is out of range.

Because we will return a value from our mock calls, we need to provide details of the expected call to Cotest by using for example `WAIT_FOR_CALL(mock_turtle, GetX)`. This allows Cotest to determine the _signature_ of the call, and hence its _return type_. Using this form has another effect: if the wrong mock call is seen at this point, Cotest will `drop` the call. More on this later.

```
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
```
This example demonstrates Cotest's _linearity_ property: information showing to how the test will function as it runs is laid out in time order, form first to last. Stimulus (in this case return values of mock calls) appears immediately before checking (of the necxt event: either a mock call to `GoTo()` or completion of the launch.

Of course, the user is free to break linearity by adding loops or function calls to the test body. _Please note that function calls containing any of the upper-case Cotest commands will usually not be compatible with C++20 coroutines when support for these is added._

#### Loop inside test case example

Here we give an example of a Cotest test containing a loop

```
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
```
Cotest helps us to paint a picture of an expectation that is "framed" by `PenDown()` and `PenUp()` and also contains a loop with a slightly non-trival body (two mock calls).

#### Flexible test example

Suppose we decide that continuing to turn by 90 degrees and draw another line is acceptable as long as it only over-paints what was already there. In Cotest, we will want to chenge the behaviour of the test case, based upon the _events_ (mock calls or launch completions) we receive.

```
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
```
## Adding expectations to Cotest tests
[Working With GMock](/coroutines/docs/working-with-gmock.md) will cover interoperation between GMock and Cotest features, but we will dip our toes in here. Suppose we want to allow some number of calls to some new mock call, but the current test case does not need to verify these calls. 

#### Watch then expect example

The solution in GMock is to add a separate expectation for these calls, or use `ON_CALL()`. In Cotest, we can do the same:
```
COTEST(PainterTest, SquareInkChecks1)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();
    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());

    auto l = LAUNCH( painter.DrawSquareInkChecks(5) );
    ... From here, same as the previous example ...
```
The new code-under-test method makes calls to `InkCheck()` and we want to absorb them without error. The `EXPECT_CALL()` works as per GMock and will absorb these calls. 

Since the `EXPECT_CALL()` comes after the `WATCH_CALL()` it has a higher priority. This means the coroutine will not _see_ the call if the expectation handles it, and in this case the expectation handles all calls to `InkCheck()`.

#### Expect then watch example

We could reverse the priorities if we wanted. The test case is interested in four different mock calls but not interested in a fifth, so things get a little unwieldly:
```
COTEST(PainterTest, SquareInkChecks2)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    EXPECT_CALL(mock_turtle, InkCheck).WillRepeatedly(Return());
    WATCH_CALL(mock_turtle, PenDown);
    WATCH_CALL(mock_turtle, PenUp);
    WATCH_CALL(mock_turtle, Forward);
    WATCH_CALL(mock_turtle, Turn);

    auto l = LAUNCH( painter.DrawSquareInkChecks(5) );
    ... From here, same as the previous example ...
```



# TODO
 - Trouble in SquareFlexibleCase - see issue #12
   - Then, since this is the best way to deal with InkCheck(), give this example first (i.e. `EXPECT(InkCheck); WATCH_CALL();`) and then the other way around (`WATCH_CALL(); EXPECT(InkCheck);`) as an alternative.
 - This doc needs an "info box" to explain the difference between: exterior filtering, interior filtering and just accepting every call and checking it. It should define "see" and "seen" in Cotest terminology.
 - We can leave the actual contents of the WAIT_X() mecros to the server style doc, however.

### Remaining sections
 - Section on RandomPointOnCircle which demonstrates GetArg<>()
 - Section on MultiLaunch which demonstrates _FROM etc
 - We should finish by discussing the mutex example and then linking to it.
