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
> [!WARNING]
> Cotest requires that we use `WAIT_FOR_RESULT()` or similar, in order to prove that the test case has checked for completion of the launch. This must happen before the launch handle goes out of scope, even though we don't use it directly.

We can see that the code under test has successfully modfied our local variable `i`. Thus, Cotest respects reference arguments to launches.

#### Arbitrary expression example

The `LAUNCH()` macro takes an expression, and this does not need to be of the form `<object>.<method>(<args>)`. For example, we can test an operator using its intended syntax:

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

In order to be able to handle a mock call inside a coroutine, it needs to be able to _see_ the call. If a call is made that we cannot see, Google Mock will treat it as an unhandled mock call. If we are using a `WAIT_FOR_` macro to wait for a particular call (or launch result), any non-matching mock call will again be an unhandled mock call even though the coroutine did see the call.

Let's call a code-under-test function that makes a mock call. We will 
 - Inject a dependency onto our mock object by passing a pointer to it to the code-under-test.
 - Make sure Cotest can see mock calls using `WATCH_CALL()`
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
> `IS_CALL()` and similar checking functions return the same (valid) handle on match and a null handle on false.
> It is _not_ an error to invoke checking functions on null handles - they will just return another null handle.
> This allows us to chain checking functions, and we get an _and_-rule.

In place of `WATCH_CALL()` we could have used:
 - `WATCH_CALL(mock_turtle)` to only see calls to that mock object or
 - `WATCH_CALL(mock_turtle, GoTo)` to only see calls to that method, or for example
 - `WATCH_CALL(mock_turtle, GoTo(_, 1))` or
 - WATCH_CALL(mock_turtle, GoTo(_, _)).With(Gt())` to only see calls with acceptable arguments.


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
The more restrictive forms of `WATCH_CALL()` will prevent the coroutine from seeing calls that don't match. These calls will then be dealt with by Google Mock in the same way as a call that has no matching `EXPECT_CALL()`. Indeed, `WATCH_CALL()` is the Cotest counterpart to `EXPECT_CALL()`. 

> [!TIP]
> In summry, there are three ways of "filtering" mock calls:
> 1. Arguments passed to `WATCH_CALL()` - this is called _exterior filtering_ and limits what the coroutine can _see_.
> 2. Aruments passed to `WAIT_FOR_CALL()` - this is called _interior filtering_ and limits what the coroutine will _accept_.
> 3. Checking using `EXPECT_` macros and `IS_CALL()` etc
> In the first two cases, GMock may in fact be able to handle the call in [another way](/coroutines/docs/working-with-gmock.md).

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

#### Get mock call argument example

To get mock call arguments with the correct type, we must specify the mock object and method using `WAIT_FOR_CALL()` or `IS_CALL()`. We can then use the returned handle (which we call a _signature handle_) to extract arguments with the correct type. We use `GetArg<>()` for this - it is templated on the argument number, beginning at zero.

For example

```
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
```

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

## Multiple launches

An important feature of Cotest is the ability to launch the core-under-test more than once. These will not run concurrently. Instead, due to the coroutine model, each launch will proceed to the next logical break-point when the test case allows it to. Break points are:
 - Mock calls
 - Completion

#### Multi-launch example

We will launch `DrawDot()` which makes two mock calls, but while the first of these is waiting for us to instruct Cotest to let it return,
we will launch `EmptyMethod()` which will return immediately.

```
COTEST(PainterTest, MultiLaunch)
{
    MockTurtle mock_turtle;
    Painter painter(&mock_turtle);
    WATCH_CALL();

    auto l1 = LAUNCH( painter.DrawDot() );
    auto c1 = WAIT_FOR_CALL_FROM(mock_turtle, PenDown, l1);
    
    auto l2 = LAUNCH( painter.EmptyMethod() );
    WAIT_FOR_RESULT_FROM(l2);

    c1.RETURN();
    
    WAIT_FOR_CALL(mock_turtle, PenUp).RETURN();
    WAIT_FOR_RESULT_FROM(l1);
}
```
Notice the use of `WAIT_FOR_CALL_FROM()` in this example. The third argument specifies the launch session the call should come from, making for a stricter filtering criterion. 
Now that we are using more than one of these, it can be advisable to use this form. The function `From(launch)` may be used on any event, mock call or result handle to check whether it originated from the given launch session.

#### Mutex examples

We can now look at a more complete example in which we test some code that uses a mutex to protect its data. This code has a deliberate error built into it - an assumption is made about how mutexes will behave, which isn't true.

To test this we mock the mutex's `lock()` and `unlock()` methods and launch the core-under-test twice to simualte the existence of two threads. We create multiple test cases that together explore possible behaviours of the mutex.

> [!IMPORTANT]
> Cotest launches are not concurrent. Therefore, a Cotest test will never be a full test of thread safety. It is recommended to use a tool such as Thread Sanitiser or Valgrind. So:
>  - That tool is checking for undefined behaviour in the code which would not be repeatable.
>  - Once this has passed and it _is_ repeatable, Cotest is testing the _logic_ of the code.

From comments in the code, here is a description of the deliberate bug we have introoduced for Cotest to discover:

```
    /* The problem with this class:
     * We know that Example1() is always called before Example2(), so
     * we only need to test with that scenario. The implementation anticipates
     * the "medium" difficulty case, in which the methods overlap and
     * Example2() has started to run and then been blocked on the
     * mutex by Example1(), by placing the var_x increment
     * in Example2() at the end, apparently forcing the correct
     * sequence of events. But this is wrong, and the "hard" test case
     * discovers the problem.
     */
```

We find the problem using Cotest:
 - First giving the mutex the most unsurprising behaviour (lock aquired immediately absent contention).
 - Next we make the behaviour more and more surprising while never coding a behaviour that would be illegal for the mutex.
 - We keep going until we've made test cases that excercise all the surprising but legal corner cases.

Caveats:
The assumption that `Example1()` is always started first is deliberately vague, and only serves to simplify the example. A sanitiser or Valgrind might discover the unprotected `var_x` but I believe the example could be recoded using a second mutex to prevent `var_x`; the bug would then be an assumption about the order in which competing threads acquire the two mutexes.

Please see [the Cotest mutex example](coroutines/test/cotest-mutex.cc).
