#!/bin/bash

# Setup script for FSMgine profiling tools
set -e

echo "Setting up profiling tools for FSMgine..."

# Detect OS
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
else
    echo "Cannot detect OS. Please install tools manually."
    exit 1
fi

# Install Google Benchmark
install_benchmark() {
    echo "Installing Google Benchmark..."
    
    if [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]]; then
        sudo apt-get update
        sudo apt-get install -y libbenchmark-dev
    elif [[ "$OS" == *"CentOS"* ]] || [[ "$OS" == *"Red Hat"* ]] || [[ "$OS" == *"Fedora"* ]]; then
        if command -v dnf &> /dev/null; then
            sudo dnf install -y google-benchmark-devel
        else
            sudo yum install -y google-benchmark-devel
        fi
    else
        echo "Package manager not supported. Building from source..."
        build_benchmark_from_source
    fi
}

# Build Google Benchmark from source
build_benchmark_from_source() {
    echo "Building Google Benchmark from source..."
    
    # Install dependencies
    sudo apt-get install -y git cmake build-essential
    
    # Clone and build
    git clone https://github.com/google/benchmark.git
    cd benchmark
    cmake -E make_directory "build"
    cmake -E chdir "build" cmake -DBENCHMARK_DOWNLOAD_DEPENDENCIES=on -DCMAKE_BUILD_TYPE=Release ../
    cmake --build "build" --config Release
    sudo cmake --build "build" --config Release --target install
    cd ..
    rm -rf benchmark
}

# Install profiling tools
install_profiling_tools() {
    echo "Installing profiling tools..."
    
    if [[ "$OS" == *"Ubuntu"* ]] || [[ "$OS" == *"Debian"* ]]; then
        sudo apt-get install -y \
            linux-tools-common \
            linux-tools-generic \
            linux-tools-$(uname -r) \
            valgrind \
            gprof \
            perf-tools-unstable || true
    elif [[ "$OS" == *"CentOS"* ]] || [[ "$OS" == *"Red Hat"* ]] || [[ "$OS" == *"Fedora"* ]]; then
        if command -v dnf &> /dev/null; then
            sudo dnf install -y perf valgrind gprof
        else
            sudo yum install -y perf valgrind gprof
        fi
    fi
}

# Check if benchmark is available
if ! pkg-config --exists benchmark; then
    echo "Google Benchmark not found."
    read -p "Install Google Benchmark? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_benchmark
    fi
else
    echo "Google Benchmark already installed."
fi

# Check if profiling tools are available
if ! command -v perf &> /dev/null; then
    echo "Profiling tools not found."
    read -p "Install profiling tools (perf, valgrind, gprof)? (y/n): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        install_profiling_tools
    fi
else
    echo "Profiling tools already available."
fi

echo ""
echo "Setup complete! Available profiling methods:"
echo "1. Google Benchmark: cmake -DBUILD_BENCHMARKS=ON .. && make benchmark"
echo "2. Perf: perf record ./your_program && perf report"
echo "3. Valgrind: valgrind --tool=callgrind ./your_program"
echo "4. GProf: compile with -pg, run program, then gprof"
echo ""
echo "See PROFILING.md for detailed usage instructions." 