# ThroughputTester

My demo code forked from [Silabs forum](https://www.silabs.com/community/wireless/bluetooth/knowledge-base.entry.html/2017/11/21/throughput_testerex-octu) for maintenance and further development. 
Documented also in the forum.

The throughput tester example application can be used to measure the Bluetooth connection bitrate when using Silicon Labs Bluetooth Low Energy stack. 
GATT operations, PHY and client device types (embedded, mobile, desktop) can be varied. Provides a basis for testing BLE range.

Directories:

- ncp_host: Network Co-processor host application (primary platform PC)
- soc: Embedded firmware to be run on independent chips

This started as a side project and later grew into a pretty comprehensive demo application.
Some constraints were placed on the design:

- Application should be easily importable to Silabs empty (Eclipse) project.
- Both BLE Server and Client roles should be implemented in the same firmware. Role choice at runtime.
- Application should work with different generations of Silabs chips.
- BLE Server application should be client agnostic (Mobile, Desktop, Embedded, Network Co-processor (NCP), Web Bluetooth).
- Evaluation board should have an LCD & Button interface using the Silabs provided graphics library.
- Network Co-processor used with the CLI application should not do on-chip calculations of bitrate.

Improvements I would make in hindsight:

- [ ] Remove or hide much of the global state into separate modules and data structures.
- [ ] Extract more common functionality into functions and inline when possible.
- [ ] Make the LCD graphics abstractions better suited for this application.
- [ ] Change NCP mode command line interface to use something like argp instead of my own pyramid of doom (This CLI was an added requirement mid-project).
- [ ] Change variable names to follow snake-case and make their names more declarative.
- [ ] Incorporate automated testing.
- [ ] (Change project language to C++ and make use of compile-time evaluation)