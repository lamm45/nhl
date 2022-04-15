# Introduction
This repository contains a C library and a command-line application for accessing and displaying scores from the [National Hockey League (NHL)](https://www.nhl.com/).

The library automatically downloads data from the publicly available NHL Stats API and features a disk cache that aims to reduce the amount of data downloaded.
The command-line app can display scores for any given dates in a few different formats.
In particular, the app is capable of emulating [page 235](https://yle.fi/aihe/tekstitv?P=235) of the teletext service (a.k.a. "Teksti-TV") provided by the Finnish Broadcasting Company Yle.

The library, `libnhl`, is written in ANSI C and it has three dependencies:
[cJSON](https://github.com/DaveGamble/cJSON), [CURL](https://curl.se) and [SQLite](https://www.sqlite.org).

The command-line app, `nhl`, is written in C99. In addition to `libnhl`, the app depends on the [GNU C library](https://www.gnu.org/software/libc/).

As of April 2022, neither the library nor the application has reached version 0.0.1 yet.
Moreover, the library will not be considered stable even in the future, as it relies on the NHL Stats API which is undocumented.


# Quick Start
In Ubuntu 20.04 or later, all required dependencies can be installed by:
```
sudo apt install build-essential libcjson-dev libcurl4-openssl-dev libsqlite3-dev
```
The library and the command-line application can then be compiled simply by running
```
make
```
from the repository root folder.
The shared object file `libnhl.so` and the executable `nhl` will be placed in their respective source directories `lib` and `src`.
To execute the application, simply run
```
src/nhl
```
from the repository root folder. Run `src/nhl --help` to see all available command-line options.

The easiest way to "install" `nhl` is to create a symbolic link to it and put the link in a folder which is in `$PATH`.
For example, if `<repo>` is the repository root folder,
```
ln -s <repo>/src/nhl ~/bin/nhl
```
should work, assuming that `~/bin` exists and is in `$PATH`.
Now, you can run `nhl` from anywhere and the program should start.

# Screenshot

![screenshot](/doc/screenshot_2022-04-03.png)

The screenshot displays whatever font was used in the terminal.
As a comparison, the original Teksti-TV looked like this on April 3, 2022:

![Teksti-TV 235](/doc/tekstitv_2022-04-03.png)



# Hello World
Below is a simple example application that uses `libnhl`.
Error checking etc. are omitted for simplicity.
```c
#include <stdio.h>
#include <nhl/nhl.h>

int main(int argc, char **argv) {
    // Initialization (note: cache is disabled)
    Nhl *nhl = nhl_init(NULL);

    // Data extraction
    NhlDate date = {.year=2022, .month=1, .day=3};
    NhlSchedule *schedule;
    nhl_schedule_get(nhl, &date, NHL_QUERY_FULL, &schedule);

    // Print
    for (int i=0; i!=schedule->num_games; ++i) {
        NhlGame *game = schedule->games[i];
        printf("%s - %s : %d - %d\n", game->away->team_name, game->home->team_name,
            game->away_score, game->home_score);
    }

    // Clean-up and exit
    nhl_schedule_unget(nhl, schedule);
    nhl_close(nhl);
    return 0;
}
```

If you save the above code in a file `helloworld.c`, you can compile it by:
```
cc helloworld.c -o helloworld -I<repo>/include -L<repo>/lib -lnhl
```
Again, `<repo>` denotes path to the repository root folder.
The compiled program can then be run:
```
LD_LIBRARY_PATH=<repo>/lib ./helloworld
```
This should print the following output for the (only) game that was played on Jan 3, 2022:
```
Oilers - Rangers : 1 - 4
```
You can try to show other games by modifying the `date` variable.


# Dockerfile
The purpose of the Dockerfile is to provide a clean environment, e.g., for debugging.
It is not recommended to normally use `nhl` from a Docker container.
In particular, the disk cache will not work (unless you provide persisent storage) and times may be displayed using a wrong time zone.

Anyway, the Docker image (here called `nhltest`) can be built as follows:
```
docker build -t nhltest .
```
To start a container, run:
```
docker run -it nhltest
```
Now, commands such as `nhl` and `nhl --help` should be available.


# Other Relevant Projects
https://gitlab.com/dword4/nhlapi : Unofficial documentation for the NHL Stats API.

https://teksti-tv-235.firebaseapp.com : An online Teksti-TV emulator supporting advanced statistics and standings.

https://github.com/peruukki/nhl-recap + https://github.com/peruukki/nhl-score-api : An online app for showing animated NHL scores, together with its back-end.


# Legal Notices
The software in this repository is licensed under the MIT license.
See `LICENSE` file for details.

This project is **not** affiliated with the National Hockey League (NHL).
You should consult the legal terms of NHL to learn how to use the data that is accessible using (but not provided by) the software in this repository.
