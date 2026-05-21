// Akar Script — C++ Embedding API Demo
// Shows how to embed Akar into a C++ application.
//
// Build: cmake --build build --target embed_demo
// Run:   ./build/embed_demo

#include "akar/api/akar.h"
#include <cstdio>

// ─────────────────────────────────────────────────────────────
// Example 1: Basic execution and value passing
// ─────────────────────────────────────────────────────────────

void example_basic() {
    printf("=== Example 1: Basic Execution ===\n");

    akar_VM* vm = akar_new_vm();

    // Execute some Akar code
    akar_Error err = akar_exec(vm, "let x = 10\nlet y = 20\nprint(\"x + y = \" + to_string(x + y))");
    if (err != AKAR_OK) {
        printf("Error: %s\n", akar_last_error(vm));
    }

    // Push a value and set it as a global
    akar_push_number(vm, 42.0);
    akar_set_global(vm, "answer");

    // Execute code that uses it
    akar_exec(vm, "print(\"The answer is: \" + to_string(answer))");

    // Read a global back
    akar_get_global(vm, "answer");
    printf("C++ side: answer = %.0f\n", akar_to_number(vm, -1));
    akar_pop(vm, 1);

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 2: Register native functions
// ─────────────────────────────────────────────────────────────

// Native: add(a, b) -> a + b
static int native_add(akar_VM* vm, int argc, int argv_base) {
    double a = akar_to_number(vm, argv_base);
    double b = akar_to_number(vm, argv_base + 1);
    akar_push_number(vm, a + b);
    return 1; // 1 return value
}

// Native: greet(name) -> "Hello, <name>!"
static int native_greet(akar_VM* vm, int argc, int argv_base) {
    const char* name = akar_to_string(vm, argv_base);
    char buf[256];
    snprintf(buf, sizeof(buf), "Hello, %s!", name);
    akar_push_string(vm, buf);
    return 1;
}

// Native: Vec2(x, y) — creates an array [x, y]
static int native_vec2(akar_VM* vm, int argc, int argv_base) {
    double x = akar_to_number(vm, argv_base);
    double y = akar_to_number(vm, argv_base + 1);

    akar_array_begin(vm);
    akar_array_push_number(vm, x);
    akar_array_push_number(vm, y);
    akar_array_end(vm);
    return 1;
}

void example_native_functions() {
    printf("=== Example 2: Native Functions ===\n");

    akar_VM* vm = akar_new_vm();

    // Register native functions
    akar_register(vm, "add", native_add);
    akar_register(vm, "greet", native_greet);
    akar_register(vm, "Vec2", native_vec2);

    // Call them from Akar
    akar_exec(vm, "print(\"add(3, 4) = \" + to_string(add(3, 4)))");
    akar_exec(vm, "print(greet(\"Akar\"))");

    akar_exec(vm,
        "let v = Vec2(10, 20)\n"
        "print(\"Vec2: [\" + to_string(v[0]) + \", \" + to_string(v[1]) + \"]\")\n"
    );

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 3: Call Akar functions from C++
// ─────────────────────────────────────────────────────────────

void example_call_function() {
    printf("=== Example 3: Call Akar Functions from C++ ===\n");

    akar_VM* vm = akar_new_vm();

    // Define a function in Akar
    akar_exec(vm,
        "fn factorial(n) {\n"
        "    if (n <= 1) return 1\n"
        "    return n * factorial(n - 1)\n"
        "}\n"
    );

    // Also call using C++ RAII wrapper
    {
        akar_api::VM cpp_vm;
        cpp_vm.exec(
            "fn factorial(n) {\n"
            "    if (n <= 1) return 1\n"
            "    return n * factorial(n - 1)\n"
            "}\n"
        );
        cpp_vm.push(10);
        int nresults = cpp_vm.call("factorial", 1);
        if (nresults > 0) {
            printf("factorial(10) = %.0f (via C++ wrapper)\n", cpp_vm.to_number(-1));
            cpp_vm.pop(nresults);
        }
    }

    // Also call using C API
    akar_push_number(vm, 20);
    int n = akar_call(vm, "factorial", 1);
    if (n > 0) {
        printf("factorial(20) = %.0f\n", akar_to_number(vm, -1));
        akar_pop(vm, n);
    }

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 4: Build arrays and maps from C++
// ─────────────────────────────────────────────────────────────

void example_arrays_maps() {
    printf("=== Example 4: Arrays & Maps ===\n");

    akar_VM* vm = akar_new_vm();

    // Build an array
    akar_array_begin(vm);
    akar_array_push_number(vm, 10);
    akar_array_push_number(vm, 20);
    akar_array_push_number(vm, 30);
    akar_array_push_string(vm, "hello");
    akar_array_push_bool(vm, 1);
    akar_array_end(vm);
    akar_set_global(vm, "my_array");

    akar_exec(vm,
        "print(\"Array length: \" + to_string(len(my_array)))\n"
        "for i in 0..len(my_array) - 1 {\n"
        "    print(\"  [\" + to_string(i) + \"] = \" + to_string(my_array[i]))\n"
        "}\n"
    );

    // Build a map
    akar_map_begin(vm);
    akar_map_set_number(vm, "x", 100.0);
    akar_map_set_number(vm, "y", 200.0);
    akar_push_string(vm, "Player1");
    akar_map_set_string(vm, "name");
    akar_map_end(vm);
    akar_set_global(vm, "my_map");

    akar_exec(vm,
        "print(\"Map x = \" + to_string(my_map.x))\n"
        "print(\"Map y = \" + to_string(my_map.y))\n"
    );

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 5: Register a module (group of functions)
// ─────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────
// Example 5: Register a module (group of functions)
// ─────────────────────────────────────────────────────────────

static int math_lerp_fn(akar_VM* vm, int argc, int argv_base) {
    // When called as math.lerp(0, 100, 0.5), argc=4, argv[0]=math (this)
    // When called as lerp(0, 100, 0.5), argc=3
    int offset = (argc == 4) ? 1 : 0; // skip 'this' for method calls
    double a = akar_to_number(vm, argv_base + offset);
    double b = akar_to_number(vm, argv_base + offset + 1);
    double t = akar_to_number(vm, argv_base + offset + 2);
    double result = a + (b - a) * t;
    akar_push_number(vm, result);
    return 1;
}

static int math_clamp_fn(akar_VM* vm, int argc, int argv_base) {
    int offset = (argc == 4) ? 1 : 0;
    double val = akar_to_number(vm, argv_base + offset);
    double lo = akar_to_number(vm, argv_base + offset + 1);
    double hi = akar_to_number(vm, argv_base + offset + 2);
    if (val < lo) val = lo;
    if (val > hi) val = hi;
    akar_push_number(vm, val);
    return 1;
}

static int math_lerp_degrees_fn(akar_VM* vm, int argc, int argv_base) {
    int offset = (argc == 4) ? 1 : 0;
    double a = akar_to_number(vm, argv_base + offset);
    double b = akar_to_number(vm, argv_base + offset + 1);
    double t = akar_to_number(vm, argv_base + offset + 2);
    double diff = b - a;
    if (diff > 180.0) diff -= 360.0;
    if (diff < -180.0) diff += 360.0;
    double result = a + diff * t;
    if (result < 0) result += 360.0;
    if (result >= 360) result -= 360.0;
    akar_push_number(vm, result);
    return 1;
}

void example_modules() {
    printf("=== Example 5: Module Registration (as direct natives) ===\n");

    akar_VM* vm = akar_new_vm();

    // Register math functions directly as globals
    akar_register(vm, "lerp", math_lerp_fn);
    akar_register(vm, "clamp", math_clamp_fn);

    akar_exec(vm,
        "print(\"lerp(0, 100, 0.5) = \" + to_string(lerp(0, 100, 0.5)))\n"
        "print(\"clamp(150, 0, 100) = \" + to_string(clamp(150, 0, 100)))\n"
    );

    // Also register as a module (map of functions)
    static const akar_ModuleFunc math_module[] = {
        { "lerp",         math_lerp_fn },
        { "clamp",        math_clamp_fn },
        { "lerp_degrees", math_lerp_degrees_fn },
        { NULL, NULL }
    };
    akar_register_module(vm, "math", math_module);

    akar_exec(vm,
        "print(\"math.lerp(0, 100, 0.5) = \" + to_string(math.lerp(0, 100, 0.5)))\n"
        "print(\"math.clamp(150, 0, 100) = \" + to_string(math.clamp(150, 0, 100)))\n"
    );

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 6: Game loop pattern
// ─────────────────────────────────────────────────────────────

static int get_time_fn(akar_VM* vm, int /*argc*/, int /*argv_base*/) {
    static double t = 0.0;
    t += 0.016; // ~60fps
    akar_push_number(vm, t);
    return 1;
}

void example_game_loop() {
    printf("=== Example 6: Game Loop Pattern ===\n");

    akar_VM* vm = akar_new_vm();

    // Register a "get_time" native
    akar_register(vm, "get_time", get_time_fn);

    // Define game logic in Akar
    akar_exec(vm,
        "let player_x = 0.0\n"
        "let player_speed = 100.0\n"
        "let last_time = 0.0\n"
        "\n"
        "fn game_init() {\n"
        "    player_x = 50.0\n"
        "    last_time = get_time()\n"
        "    print(\"Game initialized! Player at x=\" + to_string(player_x))\n"
        "}\n"
        "\n"
        "fn game_update() {\n"
        "    let now = get_time()\n"
        "    let dt = now - last_time\n"
        "    last_time = now\n"
        "    player_x = player_x + player_speed * dt\n"
        "    print(\"  t=\" + to_string(now) + \" player_x=\" + to_string(player_x))\n"
        "}\n"
        "\n"
        "fn game_get_x() {\n"
        "    return player_x\n"
        "}\n"
    );

    // Initialize from C++
    akar_call(vm, "game_init", 0);
    akar_pop(vm, 1);

    // Simulate 5 frames
    for (int frame = 0; frame < 5; frame++) {
        akar_call(vm, "game_update", 0);
        akar_pop(vm, 1);
    }

    // Read final state
    int n = akar_call(vm, "game_get_x", 0);
    if (n > 0) {
        printf("Final player_x = %.1f\n", akar_to_number(vm, -1));
        akar_pop(vm, n);
    }

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 7: Error handling
// ─────────────────────────────────────────────────────────────

void example_errors() {
    printf("=== Example 7: Error Handling ===\n");

    akar_VM* vm = akar_new_vm();

    // Syntax error
    akar_Error err = akar_exec(vm, "let x = ");
    if (err != AKAR_OK) {
        printf("Compile error: %s\n", akar_last_error(vm));
    }

    // Runtime error
    err = akar_exec(vm, "let a = 10 / 0");
    if (err != AKAR_OK) {
        printf("Runtime error: %s\n", akar_last_error(vm));
    }

    // Undefined variable
    err = akar_exec(vm, "print(undefined_var)");
    if (err != AKAR_OK) {
        printf("Runtime error: %s\n", akar_last_error(vm));
    }

    // Try-catch in script
    akar_exec(vm,
        "try {\n"
        "    let x = 10 / 0\n"
        "} catch (e) {\n"
        "    print(\"Caught: \" + to_string(e))\n"
        "}\n"
    );

    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Example 8: Type checking
// ─────────────────────────────────────────────────────────────

void example_types() {
    printf("=== Example 8: Type Checking ===\n");

    akar_VM* vm = akar_new_vm();

    // Push various types
    akar_push_nil(vm);
    akar_push_bool(vm, 1);
    akar_push_number(vm, 3.14);
    akar_push_string(vm, "hello");
    akar_array_begin(vm);
    akar_array_push_number(vm, 1);
    akar_array_end(vm);
    akar_map_begin(vm);
    akar_map_set_number(vm, "a", 1);
    akar_map_end(vm);

    const char* type_names[] = {"nil", "bool", "number", "string", "array", "map"};
    for (int i = 0; i < 6; i++) {
        akar_Type t = akar_type(vm, i);
        printf("  stack[%d] type=%d (%s) value=%s\n",
               i, t, type_names[i], akar_to_string(vm, i));
    }

    akar_pop(vm, 6);
    akar_free_vm(vm);
    printf("\n");
}

// ─────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────

int main() {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║  Akar Script — C++ Embedding API Demo        ║\n");
    printf("╚══════════════════════════════════════════════╝\n\n");

    example_basic();
    example_native_functions();
    example_call_function();
    example_arrays_maps();
    example_modules();
    example_game_loop();
    example_errors();
    example_types();

    printf("All examples completed!\n");
    return 0;
}
