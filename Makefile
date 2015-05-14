# ------------------------------------------------------------------------------------------------------
#				Set the names of the resulting binary codes
# ------------------------------------------------------------------------------------------------------

# Names of the Library
LIB_NAME_SHARED = build/lib/libxerus.so
LIB_NAME_STATIC = build/lib/libxerus.a

# Name of the test executable
TEST_NAME = XerusTest

# ------------------------------------------------------------------------------------------------------
#				Register source files for the xerus library      
# ------------------------------------------------------------------------------------------------------

# Register all library source files 
LIB_SOURCES = $(wildcard src/xerus/*.cpp)
LIB_SOURCES += $(wildcard src/xerus/*/*.cpp)

# Register all unit unit test source files 
UNIT_TEST_SOURCES = $(wildcard src/unitTests/*.cpp)

# Create lists of the corresponding objects and dependency files
LIB_OBJECTS = $(LIB_SOURCES:%.cpp=build/.libObjects/%.o)
LIB_DEPS    = $(LIB_SOURCES:%.cpp=build/.libObjects/%.d)

TEST_OBJECTS = $(LIB_SOURCES:%.cpp=build/.testObjects/%.o)
TEST_DEPS    = $(LIB_SOURCES:%.cpp=build/.testObjects/%.d)

UNIT_TEST_SOURCES_OBJECTS = $(UNIT_TEST_SOURCES:%.cpp=build/.unitTestObjects/%.o)
UNIT_TEST_SOURCES_DEPS    = $(UNIT_TEST_SOURCES:%.cpp=build/.unitTestObjects/%.d)

# ------------------------------------------------------------------------------------------------------
#		Load the configurations provided by the user and set up general options
# ------------------------------------------------------------------------------------------------------
include config.mk

include makeIncludes/general.mk
include makeIncludes/warnings.mk
include makeIncludes/optimization.mk

# ------------------------------------------------------------------------------------------------------
#		  			Set additional compiler options                
# ------------------------------------------------------------------------------------------------------

# SUGGEST_ATTRIBUTES = TRUE 		# Tell the compiler to suggest attributes

# OPTIMIZE += -fprofile-generate	# Generate Profile output for optimization 
# OPTIMIZE += -fprofile-use		# Use a previously generated profile output

OTHER += -std=c++11			# Use old C++11 standard
OTHER += -D MISC_NAMESPACE=xerus	# All misc function shall live in xerus namespace

# ------------------------------------------------------------------------------------------------------
#					Load dependency files
# ------------------------------------------------------------------------------------------------------

-include $(LIB_DEPS)
-include $(TEST_DEPS)
-include $(UNIT_TEST_SOURCES_DEPS)

# ------------------------------------------------------------------------------------------------------
#					Make Rules      
# ------------------------------------------------------------------------------------------------------

# Convinience variables
FLAGS = $(strip $(LOGGING) $(DEBUG) $(WARNINGS) $(OPTIMIZE) $(ADDITIONAL_INCLUDE) $(OTHER))
MINIMAL_DEPS = Makefile makeIncludes/general.mk makeIncludes/warnings.mk makeIncludes/optimization.mk config.mk

help:
	@printf "Possible make targets are:\n \
	\t\tall \t\t\t -- Build xerus both as a shared and a static library.\n \
	\t\t$(LIB_NAME_SHARED) \t\t -- Build xerus as a shared library.\n \
	\t\t$(LIB_NAME_STATIC) \t\t -- Build xerus as a static library.\n \
	\t\ttest \t\t\t -- Build and run the xerus unit tests.\n \
	\t\tinstall \t\t -- Install the shared library and header files (may require root).\n \
	\t\t$(TEST_NAME) \t\t -- Only build the xerus unit tests.\n \
	\t\tclean \t\t\t -- Remove all object, library and executable files.\n \
	\t\tbenchmark \t\t -- Build a loosly related benchmark program.\n \
	\t\tselectFunctions \t -- Performe tests to determine the best basic array functions to use on this machine.\n"


all: $(LIB_NAME_SHARED) $(LIB_NAME_STATIC)
	mkdir -p build/include/ 
	cp include/xerus.h build/include/
	cp -r include/xerus build/include/

$(LIB_NAME_SHARED): $(MINIMAL_DEPS) $(LIB_SOURCES)
	mkdir -p $(dir $@)
	$(CXX) -shared -fPIC -Wl,-soname,libxerus.so $(FLAGS) $(LIB_SOURCES) $(CALLSTACK_LIBS) -o $(LIB_NAME_SHARED) 

# Support non lto build for outdated systems
ifdef USE_LTO
$(LIB_NAME_STATIC): $(MINIMAL_DEPS) $(LIB_OBJECTS)
	mkdir -p $(dir $@)
	gcc-ar rcs $(LIB_NAME_STATIC) $(LIB_OBJECTS)
else 
$(LIB_NAME_STATIC): $(MINIMAL_DEPS) $(LIB_OBJECTS)
	mkdir -p $(dir $@)
	ar rcs $(LIB_NAME_STATIC) $(LIB_OBJECTS)
endif

install:
	@printf "Sorry not yet supported\n" # TODO

$(TEST_NAME): $(MINIMAL_DEPS) $(UNIT_TEST_SOURCES_OBJECTS) $(TEST_OBJECTS)
	$(CXX) -D TEST_ $(FLAGS) $(UNIT_TEST_SOURCES_OBJECTS) $(TEST_OBJECTS) $(SUITESPARSE) $(LAPACK_LIBRARIES) $(BLAS_LIBRARIES) $(CALLSTACK_LIBS) -o $(TEST_NAME)

test:  $(TEST_NAME)
	./$(TEST_NAME) all

clean:
	rm -fr build
	-rm -f $(TEST_NAME)
	-rm -f include/xerus.h.gch
	

# selectFunctions: misc/preCompileSelector.cpp .obj/misc/stringUtilities.o .obj/misc/timeMeasure.o .obj/misc/namedLogger.o .obj/misc/blasLapackWrapper.o
# 	$(CXX) misc/preCompileSelector.cpp -std=c++11 -flto -fno-fat-lto-objects -flto-compression-level=0 --param ggc-min-heapsize=6442450 -Ofast -march=native \
# 	-D FULL_SELECTION_ libxerus.a $(SUITESPARSE) $(LAPACK_LIBRARIES) $(BLAS_LIBRARIES) $(CALLSTACK_LIBS) -o .obj/PreCompileSelector 
# 	.obj/PreCompileSelector

# 	
# benchmark: $(MINIMAL_DEPS) $(LOCAL_HEADERS) .obj/benchmark.o $(LIB_NAME_STATIC)
# 	$(CXX) -D CHECK_ $(FLAGS) .obj/benchmark.o $(LIB_NAME_STATIC) $(SUITESPARSE) $(LAPACK_LIBRARIES) $(BLAS_LIBRARIES) -o Benchmark
# 
# benchmarkTest: $(MINIMAL_DEPS) $(LOCAL_HEADERS) benchmark_tests.cxx $(LIB_NAME_STATIC)
# 	$(CXX) -D CHECK_ $(FLAGS) benchmark_tests.cxx $(LIB_NAME_STATIC) $(SUITESPARSE) $(LAPACK_LIBRARIES) $(BLAS_LIBRARIES) $(CALLSTACK_LIBS) -o BenchmarkTest
# 

# Build rule for normal lib objects
build/.libObjects/%.o: %.cpp $(MINIMAL_DEPS)
	mkdir -p $(dir $@) 
	$(CXX) -I include $< -c $(FLAGS) -MMD -o $@

# Build rule for test lib objects
build/.testObjects/%.o: %.cpp $(MINIMAL_DEPS)
	mkdir -p $(dir $@)
	$(CXX) -D TEST_ -I include $< -c $(FLAGS) -MMD -o $@

# Build rule for unit test objects
ifndef USE_CLANG
build/.unitTestObjects/%.o: %.cpp $(MINIMAL_DEPS) build/.preCompileHeaders/xerus.h.gch
	mkdir -p $(dir $@)
	$(CXX) -D TEST_ -I build/.preCompileHeaders $< -c $(FLAGS) -MMD -o $@
else
build/.unitTestObjects/%.o: %.cpp $(MINIMAL_DEPS)
	mkdir -p $(dir $@)
	$(CXX) -D TEST_ -I include $< -c $(FLAGS) -MMD -o $@
endif

# Build rule for the preCompileHeader
build/.preCompileHeaders/xerus.h.gch: include/xerus.h $(MINIMAL_DEPS)
	mkdir -p $(dir $@)
	$(CXX) -D TEST_ $<    $(FLAGS) -MMD -o $@

