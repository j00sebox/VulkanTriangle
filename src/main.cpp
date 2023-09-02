#include "config.hpp"
#include "Application.hpp"

#include <TaskScheduler.h>

std::vector<std::string> read_file_to_vector(const std::string& filename)
{
    std::ifstream source;
    source.open(filename);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(source, line))
    {
        lines.push_back(line);
    }
    return lines;
}

struct TaskSet : enki::ITaskSet
{
    void ExecuteRange(enki::TaskSetPartition range, u32 thread_num) override
    {
        // do something
    }
};

struct PinnedLoopTask : enki::IPinnedTask
{
    void Execute() override
    {
        while ( task_scheduler->GetIsRunning() && execute )
        {
            task_scheduler->WaitForNewPinnedTasks(); // this thread will 'sleep' until there are new pinned tasks
            task_scheduler->RunPinnedTasks();
        }
    }

    enki::TaskScheduler* task_scheduler;
    bool execute = false;
};

struct AsyncLoadTask : enki::IPinnedTask
{
    void Execute() override
    {

    }
};

int main(int argc, char** argv)
{
    // create a task scheduler with 4 threads
    enki::TaskSchedulerConfig scheduler_config;
    scheduler_config.numTaskThreadsToCreate = 4;

    enki::TaskScheduler task_scheduler;
    task_scheduler.Initialize(scheduler_config);

    // has a set size of 1 by default
    TaskSet task;
    task_scheduler.AddTaskSetToPipe(&task);

    // runs immediately since there is no other tasks being used
    task_scheduler.WaitforTask(&task);

    // create IO task, this will be running in a loop on the last thread
    PinnedLoopTask pinned_loop_task;
    pinned_loop_task.threadNum = task_scheduler.GetNumTaskThreads() - 1;

    AsyncLoadTask async_load_task;
    async_load_task.threadNum = task_scheduler.GetNumTaskThreads() - 1;
    task_scheduler.AddPinnedTask(&async_load_task);

    std::string scene_name = (argc > 1 ) ? argv[1] : "../scene.txt";
    std::vector<std::string> scene = read_file_to_vector(scene_name);

    Application app(1300, 1000);
    app.load_scene(scene);

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}