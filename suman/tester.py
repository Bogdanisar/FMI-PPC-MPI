#!/usr/bin/python3

import subprocess
import sys
import copy
import os
import os.path
import shutil
import argparse


TEST_DIR_NAME = "Tests"
INPUT_FILE_NAME = "suman.in"
OUTPUT_FILE_NAME = "suman.out"


def runCommand(cmd):
    if args.verbose >= 1: print(f"Running command: {cmd}")

    proc = subprocess.Popen(["bash", "-c", cmd], stdin=None, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    (cmdOutput, _) = proc.communicate()

    cmdOutput = cmdOutput.split("\n")
    cmdOutput[0] = "    " + cmdOutput[0]
    cmdOutput = "\n    ".join(cmdOutput)

    if args.verbose >= 2: print(cmdOutput)
    if args.verbose >= 1: print()

    if proc.returncode != 0:
        print(f"Command ({cmd}) failed!")
        sys.exit(-1)


commandCompileSequential = "g++ -std=c++11 '{file}.cpp' -o '{file}.exe' -lgmpxx -lgmp && chmod 755 '{file}.exe'"
commandCompileMPI = "mpicxx '{file}.cpp' -o '{file}.exe' -lgmpxx -lgmp && chmod 755 '{file}.exe'"
commandRunSequential = "'{file}.exe' 0"
commandRunMPI = "mpirun -n {proc_num} '{file}.exe' 0"


kExecFileName = "kExecFileName"
kExecIsMPI = "kExecIsMPI"
kExecIsBigNumber = "kExecIsBigNumber"
executables = [
    {
        kExecFileName: "suman_sequential",
        kExecIsMPI: False,
        kExecIsBigNumber: False
    },
    {
        kExecFileName: "suman_reduce",
        kExecIsMPI: True,
        kExecIsBigNumber: False
    },
    {
        kExecFileName: "suman_dynamic",
        kExecIsMPI: True,
        kExecIsBigNumber: False
    },
]


def updateExecutables():
    for exec in executables[:]:
        new_exec = copy.deepcopy(exec)
        new_exec[kExecFileName] = new_exec[kExecFileName] + "_bigNumber"
        new_exec[kExecIsBigNumber] = True
        executables.append(new_exec)


def compileExecutables():
    for executableDict in executables:
        cwd = os.getcwd()
        execPath = os.path.join(cwd, executableDict[kExecFileName])

        if executableDict[kExecIsMPI]:
            compileCommand = commandCompileMPI.format(file=execPath)
        else:
            compileCommand = commandCompileSequential.format(file=execPath)

        runCommand(compileCommand)


def copyTestContentsToInput(testName):
    cwd = os.getcwd()
    testsDir = os.path.join(cwd, TEST_DIR_NAME)
    testPath = os.path.join(testsDir, testName)
    inputPath = os.path.join(cwd, INPUT_FILE_NAME)

    if args.verbose >= 1: print(f"Copying file contents from test ({testPath}) to input file ({inputPath})\n")

    shutil.copyfile(testPath, inputPath)


def runTests():
    cwd = os.getcwd()
    testsDir = os.path.join(cwd, TEST_DIR_NAME)
    output_file = os.path.join(cwd, OUTPUT_FILE_NAME)

    limit = 1 # TODO: Remove
    allTests = sorted(os.listdir(TEST_DIR_NAME))
    for i in range(0, len(allTests)):
        testName = allTests[i]

        if args.test_name is not None and testName not in args.test_name:
            # if args.verbose >= 1: print(f"Test #{i} is not in the list. Skipping...\n\n")
            continue

        # run each test multiple times to double-check for race conditions
        for c in range(4):
            print(f"🟡🟡🟡🟡 Running test #{i} {testName}! 🟡🟡🟡🟡")
            testPath = os.path.join(testsDir, testName)

            copyTestContentsToInput(testName)

            testIsBigNumber = ("bigNumber" in testPath)
            if testIsBigNumber:
                print(f"🟡 Test #{i} {testName} is a bigNumber test! 🟡\n")

            if args.big is not None:
                if args.big != testIsBigNumber:
                    print("Skipping...\n")
                    continue

            results = set()
            for executableDict in executables:
                executablePath = os.path.join(cwd, executableDict[kExecFileName])

                executableIsBigNumber = executableDict[kExecIsBigNumber]
                if testIsBigNumber and not executableIsBigNumber:
                    continue

                if executableDict[kExecIsMPI]:
                    procNumList = [2,4,8]
                    cmd = commandRunMPI
                else:
                    procNumList = [1] # doesn't matter. We just want a list with one element
                    cmd = commandRunSequential

                for processNumber in procNumList:
                    modified_cmd = cmd.format(file=executablePath, proc_num=processNumber)
                    runCommand(modified_cmd)

                    curr_result = str(open(output_file).read())
                    results.add(curr_result.strip())

            if len(results) == 1:
                print(f"📗 Test #{i} ({testName}) succeeded with result '{results.pop()}'! ")
                print()
                print()
            else:
                print(f"Multiple results: {results}")
                print(f"📙 Test #{i} ({testName}) failed! ")
                sys.exit(-1)


if __name__ == "__main__":
    abspath = os.path.abspath(__file__)
    dname = os.path.dirname(abspath)
    os.chdir(dname)

    desc = ""
    desc = desc + "Run some correctness tests for all Suman implementations. "
    desc = desc + "We take inputs from ./Tests and check that they are all equal,"
    desc = desc + "the assumption being that it's unlikely for all of them to be wrong and also give the same answer."
    parser = argparse.ArgumentParser(description=desc)
    parser.add_argument('--verbose', '-v',
                        action="count",
                        default=0,
                        help='Verbosity level (0 means none)')
    parser.add_argument('--test-name', '-tname',
                        action="append",
                        type=str,
                        help='Test ID that you want to run instead of all of them. Can use multiple times.')
    parser.add_argument('--big', '-b',
                        type=int,
                        default=None,
                        help='Set to 0 or 1 to run non-big-number or big-number tests respectively')

    global args
    args = parser.parse_args()
    print(f"{args=}\n")

    updateExecutables()
    compileExecutables(); print()
    runTests()

