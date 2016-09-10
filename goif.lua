--
-- goif
--
-- Copyright (c) 2016 BearishMushroom
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the MIT license. See LICENSE for details.
--

local goif = goif or {}

goif.frame = 0
goif.active = false
goif.canvas = nil
goif.width = 0
goif.height = 0
goif.data = {}

local path = (...):match("(.-)[^%.]+$")

local libname = 'none'

local argPath = love.arg.options.game.arg[1]
if argPath == '.' then
  argPath = './'
end

if love.system.getOS() == "Windows" then
  if jit.arch == 'x86' then
    libname = 'libgoif_32.dll'
  elseif jit.arch == 'x64' then
    libname = 'libgoif_64.dll'
  else
    error("GOIF: Unsupported CPU arch.")
  end
  path = argPath .. path:gsub('(%.)', '/')
elseif love.system.getOS() == "Linux" then
  if jit.arch == 'x64' then
    libname = 'libgoif.so'
  else
    error("GOIF: Unsupported CPU arch.")
  end
  path = './' .. argPath .. path:gsub('(%.)', '/')
elseif love.system.getOS() == "OS X" then
  if jit.arch == 'x64' then
    libname = "libgoif.dylib"
  else
    error("GOIF: Unsupported CPU arch.")
  end
  path = argPath .. path:gsub('(%.)', "/") .. '/'
end

if libname == 'none' then
  error("GOIF: Couldn't find proper library file for current OS.")
end

local lib = package.loadlib(path .. libname, 'luaopen_libgoif')
lib = lib()

local function bind()
  love.graphics.setCanvas(goif.canvas)
  love.graphics.clear()
end

local function unbind()
  love.graphics.setCanvas()
end

goif.start = function(width, height)
  width = width or love.graphics.getWidth()
  height = height or love.graphics.getHeight()
  goif.frame = 0
  goif.data = {}
  goif.active = true
  if goif.canvas == nil then
    goif.width = width
    goif.height = height
    goif.canvas = love.graphics.newCanvas(goif.width, goif.height)
  end
end

goif.stop = function(file, verbose)
  file = file or "gif.gif"
  verbose = verbose or false
  goif.active = false

  if verbose then
    print("Frames: " .. tostring(#goif.data))
  end

  lib.set_frames(goif.frame - 2, goif.width, goif.height, verbose) -- compensate for skipped frames

  love.filesystem.createDirectory("goif")
  for i = 3, #goif.data do -- skip 2 frames for allocation lag
    local e = goif.data[i]
    lib.push_frame(e:getPointer(), e:getSize(), verbose)
  end

  lib.save(file, verbose)
  goif.data = {}
  collectgarbage()
end

goif.start_frame = function()
  if goif.active then
    bind()
  end
end

goif.end_frame = function()
  if goif.active then
    unbind()
    love.graphics.setColor(255, 255, 255)
    love.graphics.draw(goif.canvas)
    goif.data[goif.frame + 1] = goif.canvas:newImageData(0, 0, goif.width, goif.height)
    goif.frame = goif.frame + 1
  end
end

goif.submit_frame = function(canvas)
  if goif.active then
    goif.data[goif.frame + 1] = canvas:newImageData(0, 0, goif.width, goif.height)
    goif.frame = goif.frame + 1
  end
end

return goif
