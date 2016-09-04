# goif
A minimal gif-recoding library for LÖVE.

![alt text](https://github.com/BearishMushroom/goif/blob/master/example.gif)

### Usage
Drop `goif.lua` and the library files into your project and require it.
```lua
goif = require "goif"
```

To start recording a gif, use `goif.start()`.

To stop and save your gif, use `goif.stop()`

There are two methods to actually record the frames.
* Use `goif.start_frame()` and `goif.end_frame()` and let the library handle the rest.
* Use `goif.submit_frame()` for more manual control.

You'll need to use `goif.submit_frame()` if you use any `Canvas`'s in your rendering.

#### `goif.start()`
Starts the gif recording.

#### `goif.stop([filename, verbose])`
Exports the gif to `filename (default: gif.gif)` and overwrites it if the file already exists.

If `verbose` is true, the library will print the exporting progress into the console.

#### `goif.start_frame()`
Use this before your game renders anything, marks the start of a frame.

#### `goif.end_frame()`
Tells goif to save the frame.

This will not work properly if you use `Canvas`'s' in your rendering.

#### `goif.submit_frame(canvas)`
Use this if your rendering uses `Cavnas`'s.

Submit the `Canvas` of your finished scene and goif will add it to the gif.

## Warning!
While recording, goif can use insane amounts of memory! This is all cleaned up after export, though.

Also, please note that exporting can take a few seconds and will stall the game.

# Example
Captures a gif of a rectangle moving right, printing info as it saves.

Start recording with a, stop with d.
```lua
goif = require 'goif'

x = 0

function love.update(dt)
  x = x + 100 * dt

  if love.keyboard.isDown('a') and not goif.active then
    goif.start()
  end

  if love.keyboard.isDown('d') and goif.active then
    goif.stop('mygif.gif', true)
  end
end

function love.draw()
  goif.start_frame()
  love.graphics.setColor(0, 0, 0)
  love.graphics.rectangle('fill', 0, 0, 800, 600)
  love.graphics.setColor(255, 255, 255)
  love.graphics.rectangle('fill', x, 200, 48, 48)
  goif.end_frame()
end
```

# License
This library is free software; you can redistribute it and/or modify it under the terms of the MIT license. See LICENSE for details.

# Dependencies
The C part of this library depends on Lua 5.1 (or LuaJIT)
