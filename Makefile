# Compiler
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic

# Targets
all: RobotWarz test_robot

# Object files
RobotBase.o: RobotBase.cpp RobotBase.h RadarObj.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp

Arena.o: Arena.cpp Arena.h RobotBase.h RadarObj.h
	$(CXX) $(CXXFLAGS) -c Arena.cpp

# Main executable
RobotWarz: main.cpp Arena.o RobotBase.o
	$(CXX) $(CXXFLAGS) main.cpp Arena.o RobotBase.o -ldl -o RobotWarz

# Test executable
test_robot: test_robot.cpp RobotBase.o
	$(CXX) $(CXXFLAGS) test_robot.cpp RobotBase.o -ldl -o test_robot

clean:
	rm -f *.o RobotWarz test_robot *.so

.PHONY: all clean
