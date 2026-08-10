

**Session**

- Persist audio volume across sessions with likely a session.json file.
- Persist loaded audio. If audio cannot be found, then it just goes back to empty audio, and on launch a subtle warning popup appears saying that audio cannot be found.
- Persist spectrum file. Allow the user to set the location of all their spectrum files, and then include a visualizer switcher popout that allows the user to see the names and a preview of all of their spectrums, and then select the one they want to use from there. Maybe include a feature where the user can select a moment in time during a song to be the audio frame displayed in the previews. Remove the open and new buttons from the header, and open the switcher when the logo on the top left of the screen is pressed. In that menu there is an option create or to open another file that may not live in the projects folder.

**Playback**

- Fix play/pause glitches. When paused at a certain timestamp, and then playing shifts the timestamp back less than a second. Moving the playhead and then playing starts with a second of where it was previously before switching.
- Remove stop button (back arrow does the same thing and is more intuitive), and add frame controls. I don't know if GUI frame controls are important, but pressing < and > should move forward and back a frame, while holding ctrl for example moves by 5 seconds.
- Display the framerate of the video during playback. Cap the framerate at project framerate.
- Add speed and pitch modifiers to audio. (Visualizers are commonly used in Nightcore)

**Timeline**
- Allow the timestamp to be click to edit manually.
- Remove the Import Audio button, and make the import occur when the current filename is pressed on the top right of the timeline. If no audio exists, the filename is replaced with an import icon.
- When audio is loaded, there is a circle arrow button that appears to the left of the filename which when clicked unloads any imported audio.

**Canvas**

- Allow for navigation controls. Scrolling moves the canvas up and down, shift+scrolling moves the canvas left and right, ctrl+scrolling zooms the canvas in and out. Maybe add a key combo for setting canvas to fit. Above the canvas there are controls. A zoom in and out magnifiying glass live up there next to a set to fit. There is also a display of what percentage zoomed in you are, and you can click to modify it.
- The canvas must display the visualizer at whatever the project resolution is. Zooming in will quickly distort a visualzer with a low resolution. Framerate and resolution can be changed at any time, but if the aspect ratio is changed, unpredictable things may occur. The canvas boarder must accurately represent the aspect ratio.

**Value Controls**

- Modifying a value, whether that be by dragging a slider, or typing in a value into a text box, immediately affects the canvas in real time. Clicking off of a value entry or pressing enter unfocuses the value entry and the setting was already set before that happened.
- Values can be as precise as C++ allows a floating point digit. It doesn't round unless it needs to.
- To the right of each name is a hidden info icon that only appers when the user is hovering their mouse over a rectangle that covers the name and the icon itself. If the icon hovered over, a description pops out about what that setting means and does. Maybe add a feature that when the icon is pressed, then a window pops up with a detailed description of the setting.
- Values can be linked to other values with expressions like in Davinci Resolve.
- Values can be linked to audio provider bands. Sometimes audio providers meant for a value will just have one band.

**Layers**

- Add a shape layer which can be set to a square, circle or polygon. They act like the corresponding nodes in davinci resolve. Polygon can act like bspline by editing the points on the canvas.
- Remove up, down, and delete buttons. When a layer is right clicked it can be either deleted or duplicated. Clicking and dragging a layer changes it's location within the project z axis.
