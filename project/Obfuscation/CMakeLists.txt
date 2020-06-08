cmake_minimum_required(VERSION 3.10)
include_directories(./include)
add_library(LLVMObfuscation MODULE
		CryptoUtils.cpp
		Substitution.cpp
		StringObfuscation.cpp
		BogusControlFlow.cpp
		Utils.cpp
		SplitBasicBlocks.cpp
		Flattening.cpp
		obfuscate.cpp
		MyObfPass01.cpp MyObfPass02.cpp MyObfPass03.cpp ObfuscateZero.cpp
		SimpleInvoker.cpp
		Hello.cpp
		OpcodeCounter.cpp
)
set_target_properties(LLVMObfuscation PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)
add_dependencies(LLVMObfuscation intrinsics_gen)

