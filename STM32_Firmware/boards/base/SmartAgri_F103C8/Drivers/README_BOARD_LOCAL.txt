This directory is intentionally board-local.

On first CMake configure, if HAL/CMSIS files are missing, SmartAgri copies them
from an installed STM32Cube FW_F1 repository into this directory. The compile
target then references ONLY these local files.

Expected source repository on this machine:
  D:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6

Manual preparation is also available:
  powershell -ExecutionPolicy Bypass -File scripts/windows/prepare_board_local_drivers.ps1

After preparation CLion should NOT show an external:
  D:/STM32Cube/Repository/.../Drivers/STM32F1xx_HAL_Driver/Src
source root under the smart_agri_comm target.
