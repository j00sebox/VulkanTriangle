# VulkanTriangle

## About

This is a project I started to learn how to draw a triangle using Vulkan and it it turned into an engine that can draw many triangles. I am using this repo as a sort of testing ground for Vulkan and rendering engine architecture before I switch the backend of my other engine ToyBox from OpenGL to Vulkan. I am still actively working on this as I am still actively learning more advanced Vulkan.

![7 Bunnies](/screenshots/7_Bunnies.png)

## Requirements

Currently I have only tested it on Linux so far but there shouldn't be any reason this could also work on Windows.

## Build and Run

There is a python script that will run cmake and create the build folder:

```
python project_config.py
```
After that is pretty straight forward:

```
cd build
make
./VulkanTriangle
```
It also accepts a command line argument in the form of a txt file, this file describes what models should be loaded and what transforms they should have. You can pass the scene file into the program like so:

```
./VulkanTriangle ../scene.txt
```

The scene.txt file is the default file used for testing and is what will be loaded if no arguments are given.

## Controls

* WASD to move around
* By holding the right mouse button you can rotate the camera.
* R is to reset the camera to the origin.
* ESC will shut the program down.
