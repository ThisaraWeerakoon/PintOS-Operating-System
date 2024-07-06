<h1 align="center">PintOS Operating System </h1>
<p align="center"><i>Implementing educational operating system</i></p>

<img src="assets/spaces_rEPCmpm8DJRtInZH7OOg_uploads_git-blob-207ad4ae1875dcad2914623f4e545e0f9ec6c664_pkuos.svg
" alt="Awesome README Templates" />

As part of the <b>CS2043 - Operating Systems</b> module, I undertook this project involving the <a href="https://web.stanford.edu/class/cs140/projects/pintos/pintos_1.html">Pintos</a> teaching operating system, which is designed for the 80x86 architecture. Pintos is a simple and small OS, making it ideal for educational purposes. Despite its simplicity, it supports essential OS concepts such as kernel threads, virtual memory, user programs, and file systems. However, its initial implementations are often incomplete or premature, so what I did was strengthen those already provided systems. I did it in two steps.

List of `PintOS Operating System` categories mentioned below

## Step 1 : Implementing the Thread System

In this segment of the project, I was provided with a minimally functional thread system. My task was to extend this system's functionality to gain a deeper understanding of synchronization problems. The primary focus was on the **threads** directory, with some additional work in the **devices** directory. Compilation of the project was carried out within the **threads** directory.

To achieve this, I utilized various synchronization techniques, including **semaphores**, **locks**, **monitors**, and **interrupt handling**. These techniques were essential in managing concurrency and ensuring that threads operated correctly without conflicts or data inconsistencies.

The following table contains the list of functions the shell should support
alongside with a brief description of what they are supposed to do.

<img src="assets/Screenshot 2024-07-06 at 17.27.42.png" alt="Awesome README Templates" />

You can see in the following screenshots how my developed operating system works for those commands giving the system information of the machine. If you are also supposed to use this project, make sure to change the name in the **'init.c'** file.

<img src="assets/screenshot1 (1).jpeg" alt="Awesome README Templates" />
<img src="assets/screenshot2 (1).jpeg" alt="Awesome README Templates" />

## Step 2: Enable programs to interact with the OS via system calls.

The base code already supports loading and running user programs, but no I/O or interactivity is possible. In this step, I enabled programs to interact with the OS via system calls. For this part of the project, I primarily worked in the ]**userprog** directory, but the task required interacting with almost every other part of Pintos.

Previously, all the code ran as part of the operating system kernel, giving test code full access to privileged system parts. Running user programs changes this dynamic. This project dealt with the consequences of allowing more than one process to run simultaneously, each with a single thread. User programs operate under the illusion that they have the entire machine, so managing memory, scheduling, and other states correctly is crucial to maintain this illusion.

Unlike the previous part, where test code was compiled directly into the kernel requiring specific function interfaces, this step involved testing the OS by running user programs. 

You can see in the following screeshot how my developed os works for the in-built test cases.You can also check how extent your implementation supports for running user programs.

<img src="assets/Screenshot 1.jpeg" alt="Awesome README Templates" />

## Source Tree Overview

Let's take a look at what's inside. Here's the directory structure that you should see in **pintos/src**:

**"threads/"**

Source code for the base kernel,which modified in step 1


**"userprog/"**

Source code for the user program loader, which modified during step 2

**"vm/"**

An almost empty directory.

**"filesys/"**

Source code for a basic file system.

**"devices/"**

Source code for I/O device interfacing: keyboard, timer, disk, etc. Need to modify the timer implementation in step 1.

**"lib/"**

An implementation of a subset of the standard C library. The code in this directory is compiled into both the Pintos kernel. In both kernel code and user programs, headers in this directory can be included using the #include <...> notation. 

**"lib/kernel/"**

Parts of the C library that are included only in the Pintos kernel. This also includes implementations of some data types that you are free to use in your kernel code: bitmaps, doubly linked lists, and hash tables. In the kernel, headers in this directory can be included using the #include <...> notation.

**"lib/user/"**

Parts of the C library that are included only in Pintos user programs. In user programs, headers in this directory can be included using the #include <...> notation.

**"tests/"**

Tests for each project. You can modify this code if it helps you test your submission.

**"examples/"**

Example user programs for use.

**"misc/"**

**"utils/"**

# Contribute

Contributions are always welcome! Please create a PR to add Github Profile.

## :pencil: License

This project is licensed under [MIT](https://opensource.org/licenses/MIT) license.

## :man_astronaut: Show your support

Give a ⭐️ if this project helped you!
Example user programs for use.

