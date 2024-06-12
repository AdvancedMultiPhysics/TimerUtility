from spack.package import *


class Timerutility(CMakePackage):

    homepage = "https://re-git.lanl.gov/xcap/oss/solvers/timerutility"
    git = "ssh://git@re-git.lanl.gov:10022/xcap/oss/solvers/timerutility.git"

    version("master", branch="master")

    
    variant("mpi", default=True, description="build with mpi")

    depends_on("cmake@3.26.0:", type="build")
    depends_on("mpi", when="+mpi")

    def cmake_args(self):
        args = [
            "-DTIMER_INSTALL_DIR=" + self.prefix,
            self.define_from_variant("USE_MPI", "mpi"),

        ]

        if self.spec.satisfies("^mpi"):
            args.append("-DCMAKE_CXX_COMPILER=mpicxx")
        return args

