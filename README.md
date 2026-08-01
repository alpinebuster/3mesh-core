# GradientspaceCore
An Open Source C++ library for geometry/mesh processing.

## Feature Set
- templated Vector Math types, eg Vector2<T>, Vector3<T>, Quaternion, Matrix2, Matrix3
- ...

# Building
A sample vcxproj file for Visual Studio is provided in `/Build/vs2022/GradientspaceCore.vcxproj`. You should be able to add this directly to a VS2022 Solution.

A CMake script is also included, with presets. The preset **Windows x64 vs2022** / **windows-x64-vs2022** will generate a Visual Studio project in `Build/cmake-win64-vs2022`.

On Linux, use `cmake --preset=linux` to generate a makefile project in `Build/cmake-linux`, then run `make`.

The CMake script is suitable to be included in higher-level scripts using add_directory().
