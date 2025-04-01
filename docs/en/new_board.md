## Addition and Adaptation of Boards

### Definition

In Tuyaopen, a board refers to a set of hardware specifically designed to facilitate development work in a particular field. In addition to the core board, it includes various hardware modules such as sensors, actuators, and expansion boards, aiming to provide developers with a complete and convenient development platform to accelerate the product development process or facilitate learning and research. Each board is bound to a specific platform.

### Board Naming

#### Mandatory Fields

- Manufacturer Name: Represents the manufacturer of the development board. For example, development boards launched by Tuya start with "TUYA".
- Chip Name: The name of the platform, such as "T2" or "T5AI".

#### Optional Fields

- Main Features or Functions of the Development Board: If the board has specific functional modules or features, such as "LCD" or "CAM".
- Series Name and Version Information: Some manufacturers have serialized products, and the series name and version number can be reflected in the naming.
- Others: Some hardware identifiers, etc.

#### Naming Rules

1. All letters must be in uppercase.
2. Fields are separated by underscores.
3. The order of field combination is "Manufacturer Name_Core Board Name_Optional Fields".

Example: TUYA_T5AI_BOARD

### Adding a New Board

Please first read the [README](../../README.md) in the root directory of tuyaopen to understand the basic operations and ensure that the required environment is correctly installed.

#### Creating a New Folder

First, enter the board directory and find the corresponding platform's board collection directory. Create a new subdirectory for the board in this collection directory.

This directory mainly stores configuration files and adaptation files related to the board's characteristics.

> Example: I want to add a development board based on the T5AI chip. According to the board naming convention, the name of this board is determined to be TUYA_T5AI_EVB.

```shell
cd boards/T5AI
mkdir -p TUYA_T5AI_EVB 
```

#### Adding a Kconfig File

The peripheral types of boards may vary, so each board requires a Kconfig file.

> Example: Add a Kconfig file to TUAY_T5AI_EVB.

```shell
cd TUAY_T5AI_EVB 
vim Kconfig
```

Add board-related configuration items and save the Kconfig file. If there are no special configurations, the Kconfig file can be empty.

```
:wq
```

#### Adding the Board to the Selection List

Modify the Kconfig file in the board collection directory. Add the board's configuration option (BOARD\_CHOICE\_\<new\_board>) to the "Choice a board" selection list and reference the path of the newly added Kconfig file. After completing this operation, you can select the target board by executing the `tos menuconfig` command in the application.

> Example: Modify the boards/T5AI/Kconfig file, add the configuration option BOARD_CHOICE_TUYA_T5AI_EVB, and add the file path "./TUYA_T5AI_EVB/Kconfig".

Return to the board/T5AI directory.

```
cd ..
```

Modify the Kconfig file.

```
Vim Kconfig
```

Add a new board option.

```shell
config BOARD_CHOICE_TUYA_T5AI_EVB
    bool "TUYA_T5AI_EVB"
    if (BOARD_CHOICE_T5AI_EVB)
    rsource "./TUYA_T5AI_EVB/Kconfig"
    endif
```

Save the Kconfig file.

```
:wq
```

### Adapting an Application

Enter the application directory you want to adapt. First, execute `tos menuconfig` to select configuration options according to the board's capabilities and characteristics. After the configuration is selected, compile the application and generate the target bin file. Burn the bin file to your board for function verification. If the function verification passes, save the current configuration as the default configuration and save this default configuration in the config folder of the application for other developers to choose.

> Example: Adapt the your_chat_bot application project based on TUYA_T5AI_EVB and generate the default configuration for this board.

Return to the root path of TuyaOpen.

```
cd ../..
```

Enter the your\_chat\_bot application.

```
cd apps/tuya.ai/your_chat_bot
```

Configure the application and select the TUYA\_BOARD\_EVB board.

```
tos menuconfig
```

Compile the project to generate the target bin file.

```
tos build
```

After the function verification passes, save the current configuration as the default configuration.

```
tos savedef
```

Save the default configuration in the config folder of the application.

```
cp app_default.config ./config/TUYA_T5AI_EVB.config
```

Next time, if a developer wants to run this application on TUYA\_T5AI\_EVB, they can directly select the configuration for this board.

```
tos config_choice
```

### Contributing Code

You can refer to the [Contribution Guide](contribute_guide.md) to merge the newly added board into the main branch of tuyaopen so that more developers can use your development board.

