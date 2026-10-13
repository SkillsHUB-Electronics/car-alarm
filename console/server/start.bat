@echo off
rem Starts the local helper server and opens the console in the default browser.
rem Use Chrome or Edge as the default browser (Web Serial is required).
cd /d "%~dp0"
start "" "http://localhost:8765"
node server.js
pause
