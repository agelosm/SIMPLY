SIMPLY is a lightweight custom implementation of an MPI-like interface, using POSIX threads underneath, created for academic purposes.

SIMPLY was implemented at Spring Semester of 2010, at University of Ioannina, under the guidance of professor Vasileios Dimakopoulos.

In order to compile and run:
* make all
* ./simplyrun 4 pi

In the example above, 'pi' is a simple parallelized pi approximation algorithm, using SIMPLY's message passing functions instead of the MPI ones.
