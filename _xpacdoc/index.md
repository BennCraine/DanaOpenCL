# Dana Compute Package

The Dana Compute package allows you to interact with compute devices on your system.

There are three main levels of abstraction in this package to interact with compute devices. Depending on your inteded application one may be better than the others.

# Utility Interfaces

The first of these is the easiest way to interact with external compute devices on your system. These are a set of utility functions commonly associated with using hardware accelleration to process large datasets. For example the RNG package can be used to generate large amounts of psudo-random numbers in paralell, and the LinearOperations interface impliments tasks such as matrix multiplication on an external compute device to leverage high levels of paralellisation.

Accessing these utilities is done in the usual Dana way of requiring them in any component you're building and calling their functions:

```
component provides App requires compute.alg.LinearOperations {

    int App:main(AppParam params[])
        {
        LinearOperations linear = new LinearOperations()
				 dec matrixOne[][] = new dec[500][500]
				 dec matrixTwo[][] = new dec[500][500]

				 // populate the matricies

				 dec matrixThree[][] = linear.matrixMultiply(matrixOne, matrixTwo)

				 return 0
        }
    }
```

# LogicalCompute Interface

The second of the abstraction levels is the LogicalCompute (link to interface page) interface. This is an easy way to get up and running transfereing data and executing instructions on an external compute device. It gives the user a space to declare, read and write variables along with loading programs that operate on the varibales that have been declared. Users of this interface need not be concerned with exactly which external devices are being used, and if more than one is being used the user of the LogicalCompute won't need to decide which physical device to write data to or execute programs on.

The utility functions discussed above typically use this interface to implement their behaviour. The general workflow is as follows:

1. Declare a set of variable
2. Write some data to the variables
3. Load a program
4. Run the program on a set of the declared variables
5. Reading the result of the executed program
6. Cleaning up the variables


```
component provides dataprocessing.Normalisation requires gpu.LogicalCompute, data.DecUtil du {
    LogicalCompute myDev

    Normalisation:Normalisation() {
        myDev = new LogicalCompute()
		/// Loading the program
        myDev.loadProgram("./resources-ext/opencl_kernels/dataprocessing/floatDiv.cl", "floatDiv")
    }

    dec[][] Normalisation:matrixDivision(dec matrix[][], dec divider) {
		/// Declaring the variables needed
        myDev.createDecMatrix("mat", matrix.arrayLength, matrix[0].arrayLength)
        myDev.createDecMatrix("out", matrix.arrayLength, matrix[0].arrayLength)
        myDev.createDecArray("divider", 1)

		///Writing data to variables that need it
        myDev.writeDecArray("divider", new dec[](divider))
				 myDev.writeDecMatrix("mat", matrix)

		///Collecting together the variables as parameters
        String params[] = new String[](new String("divider"), new String("mat"), new String("out"))
		///Running the Program
        myDev.runProgram("floatDiv", params)

		///Reading the result
        dec m[][] = myDev.readDecMatrix("out")

		///Cleaning up
        myDev.destroyMemoryArea("mat")
        myDev.destroyMemoryArea("out")
        myDev.destroyMemoryArea("divider")

        return m
    }
}
```

## Parameter and Name parity
When constructing the parameter array, the parameters need to be in the same order they appear in the loaded program, therefore it's expected that users at this level wrote or are familiar with the programs being loaded. The parameters can be named differently in Dana than in the external program code, however the name of the program in Dana <b> must match exactly </b> the name of the entry point function of the external device program. The program that was loaded in the above example can be seen below:

```
__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;

__kernel void floatDiv(__global float* divider, read_only image2d_t matrix, write_only image2d_t matrix_write) {
    int gidx = get_global_id(0);
    int gidy = get_global_id(1);

    float4 fromDevice = read_imagef(matrix, sampler, (int2)(gidx, gidy));
    float ourNumber = fromDevice[0];
    float divided = ourNumber / divider[0];

    write_imagef(matrix_write, (int2)(gidx, gidy), (float4)(divided, 0 ,0 ,0));
}
```

# Compute Interfaces
Lastly is the set of interfaces that make up the Compute component. At this level, the user can select precisly which physical devices they want to use. Cluster them together in a ComputeArray, decided which programs are built for which devices etc. Getting this set up requires a few steps which will typically follow this pattern:
