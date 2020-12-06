# Summary
* Cooperative Multiplayer
* Genre: Roughly a puzzle game
* Target Audience: puzzle-ey kind of people, maybe programmers/engineers
* A game about debugging - Like a programming game, but simulating the experience of debugging networked systems or troubleshooting (similar to our lines of work). It's about many interacting systems that fail together because single piece are behaving weirdly.
* "Faster Than Light" as a CO-OP FPS from the inside.
* There are a number of systems on the ship (life support (o2), an engine, shields, turrets, a star map, etc.) and they can all communicate with each other
* They all have a uniform interface of a text-based shell and optionally individual screens with extra info/graphs

# Game Flow
Inspired by Deep Rock Galactic I intend to have the game switch between periods of low-engagement activities ("cookie-clicker gameplay") that require planning and some communication with your team mates and high intensity "events" that require focus and quick decision making (btw. I think that makes DRG a *genius* game). The low-intensity parts (which take up the majority of the game) should live a lot off the fear or something happening soon. You should forget that "shit's going down" soon, but not for long periods of time. In DRG stray, single enemies will remind you that this is a possibility and we should try to do that too.

In this game the low-intensity parts will mainly consist of maintenance activities, which consist of visiting different systems and checking their logs and status commands/status screens. Double checking with the manual if the values are nominal or just reading up in the manual to figure out certain things and very importantly doing scans and pre-detection of hazards.

# Systems
## Terminals
They are similar to a UNIX-ey shell. You can enter a single-line command (but without options, though with some positional arguments) and will get text output in the same window. You can scroll up and down with the mouse wheel or with arrow key up/down.
When you are close enough to a terminal screen, they become interactable and you can select it by pressing left click. Then your character will move to a specified position in front of the screen and your keyboard will only be able to make text inputs (no movement or other actions anymore). Only one player at a time can interact with a terminal.
Since many ship systems may have mulitple screens, you can still use the mouse to look around (and see other players or look outside the window).
Press escape or left mouse to leave the terminal.
Terminals are the only way to interact with the ship's systems.

Every terminal allows the usage of `man`/`manual`/`help` which will list the available commands, while `man <command>` will display help for a specific command. We should consider not making this case sensitive to be easier to use.

The manual will also show help topics that will explain the inner workings of the system and how it interacts. Everything there is to know. For example `manual ships` will show the ship database in the navigation computer.

## Message Bus
The ship systems and terminals all communicate via a message bus, which is managed in C++. The systems are implemented in Lua (mostly). Every system registers with a name and every message has a type/name, you can do either `send(systemName, messageName, data)` or `publish(messageName, data)` and `subscribe(messageName)` all messages directly sent to the system or subscribed to will appear in the system's message queue.

As real life troubleshooting often involves tools like `strace` or `tcpdump`/Wireshark we could maybe expose parts of the message bus with a `bustrace` command, that will essentially be a tcpdump of the message bus of that particular system. We could also include some fake messages.

## Screens
These systems can draw vector art that can be sent to them via messages (like change color, draw line, draw rect, etc. - we will use [nanovg](https://github.com/memononen/nanovg) for this). It just draws to a texture which will be drawn in the world. You should be able to draw to them from Lua.

Maybe the additional screens can be fully customized and you can control what they show using a `monitor <number>` command. For prebuilt vector graphics displays, you can simply use `monitor <number> <name>` (e.g. `monitor <number> heatmap`). But you can monitor a log with `monitor <number> log <name>` or a sensors (with graphs!) `monitor <number> sensor <name> <name> <name> ...` and possibly a bus with `monitor <number> bustrace`. The monitor number could be written on the monitor or a common ordering scheme could be used. We could also just show a little numbered overview when typing `man monitor`. If we require a look in the manual we could also use names.

We could also allow the bridge to be a special system that simply provides a number of monitors that can monitor anything remotely (like `remotemonitor 1 reactor log <name>`).

## Maintenance
### Log-Diving
On every system there are multiple log files, which can be printed with `log <name>` and they will print in the usual log format (including date/time and severity). You will have to scan the logs for suspicious info logs, warning logs and error logs, observing timestamps and patterns. Some of these will eventually or soon lead to failure or unoptimal performance of the system. Though some of them will be false positives or sometimes you will operate systems of the ship outside of normal parameters for bandaid fixes.
Most of maintenance is going through the logs and sifting through to find interesting things. Most of the messages will just be noise though, so you will need experience and intricate knowledge of the manual to truly understand what you are seeing. Even then some messags will simply be noise. After a while the systems will be damaged and some warnings will be normal as well. Some suspicious logs should be recognizable without experience (e.g. warnings that repeat and get higher in frequency or a message that was a warning for a while and is now an error). Many maintenance operations lead to warnings and errors as well. But it should pay off to do maintenace, so the ship will not break down as likely.

The amount of logs should be so high, that you can not reasonably look at them all in detail and you will have to decide what you spend your attention on.

Maybe we want to have different log levels per source? But this might make it too easy?

## Hazards
The ways you can die in the ship are:
* A hull breach
    - Enemy Fire (detected early by: visible outside windows, star map/scans, detected too late by: fire/explosions/impacts in the ship. mitigated by: charging up shields and turrets beforehand)
    - Asteroid impact (detected early by: star map/scans, detected too late by: fire/explosions/impacts)
* Radiation sources (detected by: star map/scans, detected too late by: screens flickering, power spikes, electronics malfunctioning, mitigated by: charging up ion shields)

All of these can be mitigated by preparing the ship's systems accordingly after proper anticipation or detection of them. Though if you start the counter-measures after the you are already affected by the events, you should *sometimes* (fairly randomly) *just* make it in time (i.e. if you power up the )

## Rooms
* Each room will have a number of values assigned to it:
    - Hull integrity: If it reaches 0, the ship explodes and everyone dies
    - O2 content (breathability): If a player is in a room with too little O2 for too long, they will die. O2 will be reduced (and fuels) fire.
    - Power: keeps certain machines running, including light (can turn off whole systems)

**OPEN**: Should the sensor data maybe be in a different place than the system that controls it? Then you have to walk around or cooperate.

## Ship-Systems
### General
Some systems will have a "reboot" procecure, which will fix some problems or be required for some operations, but will take a long time. Sometimes very intransparent problems that seem very weird (intermittent failures for example) will have no other fix but a costly reboot (which is kind of a joke).

Maybe some other memes like that can be used, like a diagnostics mode or "health check", which will put the system in reduced capacity, but will print out very helpful information or maybe even a literal step-by-step guide of how to fix what you are doing, but it will take time and power and reduced capacity. Maybe you can also just not interact with the system as long as the diagnostics are running?

The amount of power you route to a system will determine how much of it you can use. E.g. If the engine runs in low-power-mode the wear will be higher and you can run *no* diagnostics. At low power levels you will have to choose the sensors you turn on:
`sensor enable <name>` may show "insufficient power"
`sensor disable <name>`
`sensor show <name>`
`man sensor` to see sensor list

Sometimes certain sensors will simply be broken and there is nothing else but to discard their data and have them repaired at the next stop (i.e. the next level).

### Reactors (will show power data)
It should be an array of reactor cores with a buffer battery. When a single core fails (melts?), the whole reactor shuts down, because some circuit is broken. You will have to identify the broken core(s) and deactivate it, then reboot the reactor to have it run with limited capacity again.
You can query the power output and fuel consumption for each core separately (weird ratios should jump out) and you may lower the fuel input for individual cores, so you can turn it down and back up quickly to "flush" it, which will clean up the core internally, but the cores will still fail either way (just later).
When the core fails you can not look at the per-core diagnostics anymore, but a core dump has been created, which you can inspect (some sort of where's-waldo type thing with hex numbers?) to guess the faulty core and reboot with it deactivated. Maybe the core dump has to be carried to another computer to be analyzed? When the reactor is offline (e.g. rebooting), you will want to power down certain systems to keep the buffer battery going for longer.

When the power is completely gone, the ship systems will simply all deactivate, except the reactor and the board computer and you will have to restart the whole ship.

### Engine

### Weapons
Only turrets, which one player has to operate manually, but they have to be charged beforehand.

### Astrodroids (will show Hull integrity data)
### Navigation-Computer / Star Map / AstraCarta (tm)
You can do different kinds of scans in different variations:
* Ship-Scan (long/short-range): will show a dot for each ship, a course-line and a ship id or type. You can consult a database to figure out what kind of factions will use these ships and also whether they might be hostile. There are also Hyperspace-Highways shown in the map and ships that stray from these highways might be problematic as well. Maybe you can specify a range and the duration is dependent on that.
* Single-Ship-Scan: Scan a specific ship to figure out number of crew and the equipment. Maybe some ships will not like you scanning them, so they will *turn* hostile after a scan. Maybe have them send you messages? Or maybe display some weird signature-string that you have to remember and recognize later als hostile or friendly. Maybe play sounds like that?
* Obstacle-Scan: Will show obstacles in a specified range

### Life Support (will also show O2 sensor data)
### Shield (combat)
### Radiation-Shield (shows geiger-data)
### Sensors
### Doors
Can be closed for hull breaches and opened to evacuate spaces in case of fire.

## Standing Still
Maybe we should provide a cloaking system that can be turned on pretty easily if the engine is off, so you can simply turn off the engine, stand still and take some time to fix a problem (while making no progress on your route though). This is like pausing in FTL. Most hazards are location-dependent, so not moving will get rid of those automatically (asteroids, solar flares).

## Alerts (very optional)
Maybe we should introduce a way to set up alerts like `alert log <logname> <string>` (fires when a string is found in a log) or `alert sensor <sensorname> less/equal/greater <value>` (self-explanatory), but there will only be a very limited number of alerts. With a customizable monitoring system this is not really necessary anymore, though. A cloak would be an option for enemies, but maybe just turning off the engines will make the ship invisible on other ships radar. We could possibly turn down the lights and change the background sound to sell the mood.

## Physical Actions (optional)
I think it would be nice if there were physical actios to perform in the ship apart from operating the turret. Repair things with tools, replace parts, open/close valves, carrying around measuring devices and most importantly plenty of **manual overrides**.

# Metagame / Setting / Story
## Tutorial / First Level
You start in a hangar/space port (can see it out of the window) and the first level is to boot up the ship. The board computer (navigation computer) will list which systems are offline and which have to be started to take off. AstraCarta will show "docked in space port XYZ". You go to the systems and enter "man" or "help" to figure out how to start the system (it's always "init" though). Some systems will complain that they depend on other systems.

Your goal is to reach a destination with some in-between stops refuelling or you have to travel between warp-stations.

In each level more and more events are happening and some systems will simply fail (or fail more) due to wear.

The game rises in difficulty also in the way that systems are relevant. In the first level (or two) you don't have to use the astrodroids and the engines never fail, so you have to manage a smaller number of systems. Everything is running smoothly so far. That also means that you *can* start learning about those systems in peace and that the ship is complete from the start.

Getting to know the ship well should be rewarded a lot and good knowledge should lead to smooth operation.

**OPEN**: If you die or a ship gets destroyed, do you start from the beginning or the start of the level? Does your ship get repaired/reset between levels?

# Vision
## Hull-Breach in Asteroid Belt
You have missed an asteroid belt and suddenly you hear an impact in the ship and notice the screen rumbling. Looking out the windows you see many small asteroids.
You power down some systems and reroute to the shield to reduce damage (it will not be sufficient) and do a short-range asteroid scan (which is relatively quick compared to long-range) to gauge how long you will be in there and if you are going to make it.
You cannot send out the astrodroids during the storm, because they will get hit and get destroyed (you can send them out though, but there is a chance they will just get destroyed. and you have to pick the right room.), so you keep watch on the hull integrity screen and figure out which room might break and which will break first. The other players do whatever task they need to do, before the might not get access to the room anymore and close the doors. You turn off the O2 supply for that room to not lose any more O2 and when the storm is done you send out the astrodroids to do repairs.

## Bandaid-Fixes
It should be possible to abuse systems to fix problems with other systems. Trade power against O2 and other resources. Or keep systems online, while putting a great strain (wear) on other systems.

## Enemy-Encounter
Enemy found on short-range scanner, immediately power down some systems (which? definitely engine), power up the turret and the shields.

# Random Notes
* Sensor data could be: heat, pressure for engine/reactor.

# Style
I bought this asset pack: https://syntystore.com/products/simple-space-interiors-cartoon-assets
And it will mostly be unlit or use prebaked lighting, because it's a fucking game jam and I am not using an engine.
The interfaces/UI should be like Alien (retrofuturistic):
* https://sciencefictioninterfaces.tumblr.com/post/107518837096/various-images-showcasing-the-beautiful-interfaces
* http://cdn4.artofthetitle.com/assets/sm/upload/4e/r6/z1/7z/AI%20In-World%20Interface%202.jpg
* https://lukas_uhlitz.artstation.com/projects/XDdyL
* https://www.youtube.com/watch?v=qb43-hn_-_c&list=PLapLmeK627G64iUuQBFDCDMzqIFX4aI1i&index=16&t=0s&app=desktop (the curve/warp might be interesting. the sounds are AMAZING and exactly what I would want)
* https://www.youtube.com/watch?v=sT9R34SRvGQ&list=PLapLmeK627G64iUuQBFDCDMzqIFX4aI1i&index=13 (same sounds, still great)
* https://gfycat.com/damphairydromedary
* https://brennan.io/2017/06/14/alien-computer-card/ everything here
* Print the name of each system at boot or as "screensaver" in big ASCII art: http://patorjk.com/software/taag/#p=display&v=0&f=Slant&t=AstraCarta%20(tm) - auch cool: Ogre, Standard, Star Wars, 3D-ASCII, ANSI Regular, ANSI Shadow, 5 Line Oblique, Banner, Banner3, Banner4,
* https://typesetinthefuture.com/2014/12/01/alien/
* https://store.steampowered.com/app/470260/Event0/
* https://futureinterface.tumblr.com/
* https://www.hudsandguis.com/home/2015/10/4/decrypt-by-peter-clark
* https://i.pinimg.com/originals/0d/e2/ff/0de2ffdfd8325d2b19a985fd30a4c6ac.png
* https://i.pinimg.com/originals/7a/42/a3/7a42a3949bad55f66a9c7c7988332171.jpg

Lots of monospace fonts, all caps blinking text. Stuff
like "EXECUTED", "LINK UP", "LINK DOWN", "INITIALIZING"
