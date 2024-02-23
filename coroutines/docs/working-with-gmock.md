# Under construction


How this doc should work
 - fix up the section ## Adding expectations to Cotest tests and begin with this but provide the whole code snippet (it's truncated in the below)
   - cover options arounf priority ordering, WATCH_CALL() and "flexible" algo
   - get to the tricky case, say that server style is needed and link to that doc where the case will be covered.
 - Start using TEST(s,c)-type tests with COROUTINE
   - Initially just suggest a layout
   - Touch on NEW_COROUTINE
   - Discuss launching from main vs from coro and the scope for confusion
 - Now do cardinality
   - Assumption of saturated+satisfied on exit
   - SATISFY()
   - RETIRE() 
 - Get to multiple coroutines (or is this a different document?)
   - Discuss their priority
   - Clarify that launches are bound but mock calls are not
   - Show one coro seeing mock calls from another coro's launch
   - Maybe link to the sheared ordering doc




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
