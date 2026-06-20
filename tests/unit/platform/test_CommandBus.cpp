#include "platform/CommandBus.h"

#include <gtest/gtest.h>

#include <string>

using namespace beamlab::platform;

// ── Dummy command / handler ────────────────────────────────────────

struct DoublerCmd { int value; };
struct DoublerRes { int result; };

class DoublerHandler : public CommandHandler<DoublerCmd, DoublerRes> {
public:
    DoublerRes handle(const DoublerCmd& cmd) override {
        return {cmd.value * 2};
    }
};

// ── Register + dispatch ────────────────────────────────────────────

TEST(CommandBusTest, DispatchRegistered)
{
    CommandBus bus;
    bus.registerHandler<DoublerCmd, DoublerRes>(
        std::make_unique<DoublerHandler>());

    auto res = bus.dispatch<DoublerCmd, DoublerRes>(DoublerCmd{21});

    EXPECT_EQ(res.result, 42);
}

TEST(CommandBusTest, DispatchMultipleTypes)
{
    struct NegateCmd { int value; };
    struct NegateRes { int result; };
    class NegateHandler : public CommandHandler<NegateCmd, NegateRes> {
    public:
        NegateRes handle(const NegateCmd& cmd) override {
            return {-cmd.value};
        }
    };

    CommandBus bus;
    bus.registerHandler<DoublerCmd, DoublerRes>(
        std::make_unique<DoublerHandler>());
    bus.registerHandler<NegateCmd, NegateRes>(
        std::make_unique<NegateHandler>());

    // Template args with commas confuse preprocessor — bind to variable first.
    auto d = bus.dispatch<DoublerCmd, DoublerRes>(DoublerCmd{10});
    EXPECT_EQ(d.result, 20);
    auto n = bus.dispatch<NegateCmd, NegateRes>(NegateCmd{10});
    EXPECT_EQ(n.result, -10);
}

// ── Error handling ─────────────────────────────────────────────────

TEST(CommandBusTest, UnregisteredThrows)
{
    CommandBus bus;
    // No handler registered for this command type.
    // Wrap in lambda to avoid preprocessor seeing the comma in template args.
    EXPECT_THROW(
        ([&]{ bus.dispatch<DoublerCmd, DoublerRes>(DoublerCmd{99}); }()),
        std::runtime_error);
}

TEST(CommandBusTest, DoubleRegistrationThrows)
{
    CommandBus bus;
    bus.registerHandler<DoublerCmd, DoublerRes>(
        std::make_unique<DoublerHandler>());

    // Trying to register another handler for the same type should throw.
    EXPECT_THROW(
        ([&]{ bus.registerHandler<DoublerCmd, DoublerRes>(
                  std::make_unique<DoublerHandler>()); }()),
        std::runtime_error);
}

// ── hasHandler ──────────────────────────────────────────────────────

TEST(CommandBusTest, HasHandlerReturnsTrueForRegistered)
{
    CommandBus bus;
    bus.registerHandler<DoublerCmd, DoublerRes>(
        std::make_unique<DoublerHandler>());

    EXPECT_TRUE(bus.hasHandler(std::type_index(typeid(DoublerCmd))));
    EXPECT_FALSE(bus.hasHandler(std::type_index(typeid(std::string))));
}

// ── Clear ───────────────────────────────────────────────────────────

TEST(CommandBusTest, ClearRemovesAllHandlers)
{
    CommandBus bus;
    bus.registerHandler<DoublerCmd, DoublerRes>(
        std::make_unique<DoublerHandler>());
    EXPECT_TRUE(bus.hasHandler(std::type_index(typeid(DoublerCmd))));

    bus.clear();
    EXPECT_FALSE(bus.hasHandler(std::type_index(typeid(DoublerCmd))));
    EXPECT_THROW(
        ([&]{ bus.dispatch<DoublerCmd, DoublerRes>(DoublerCmd{1}); }()),
        std::runtime_error);
}

// ── String-based command ────────────────────────────────────────────

TEST(CommandBusTest, StringCommand)
{
    struct ConcatCmd { std::string a; std::string b; };
    struct ConcatRes { std::string result; };
    class ConcatHandler : public CommandHandler<ConcatCmd, ConcatRes> {
    public:
        ConcatRes handle(const ConcatCmd& cmd) override {
            return {cmd.a + cmd.b};
        }
    };

    CommandBus bus;
    bus.registerHandler<ConcatCmd, ConcatRes>(
        std::make_unique<ConcatHandler>());

    auto res = bus.dispatch<ConcatCmd, ConcatRes>(ConcatCmd{"hello", " world"});

    EXPECT_EQ(res.result, "hello world");
}

TEST(CommandBusTest, HandlerReceivesCorrectCommand)
{
    CommandBus bus;
    int receivedValue = 0;

    struct CaptureCmd { int x; };
    struct CaptureRes { bool ok; };
    class CaptureHandler : public CommandHandler<CaptureCmd, CaptureRes> {
    public:
        int& captured;
        explicit CaptureHandler(int& c) : captured(c) {}
        CaptureRes handle(const CaptureCmd& cmd) override {
            captured = cmd.x;
            return {true};
        }
    };

    bus.registerHandler<CaptureCmd, CaptureRes>(
        std::make_unique<CaptureHandler>(receivedValue));

    bus.dispatch<CaptureCmd, CaptureRes>(CaptureCmd{42});
    EXPECT_EQ(receivedValue, 42);
}
