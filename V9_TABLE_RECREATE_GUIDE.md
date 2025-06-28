# V9表重新创建功能使用指南

## 功能说明

在开发阶段，当你修改了V9_SETUP_QUERIES中的表结构时，可以通过设置环境变量来重新创建V9表，而不需要删除整个数据库或通过版本升级。

## 使用方法

### 1. 设置环境变量

在启动应用前，设置以下环境变量：

**Windows (PowerShell):**
```powershell
$env:MAILSPRING_DEV_RECREATE_V9="1"
```

**Windows (CMD):**
```cmd
set MAILSPRING_DEV_RECREATE_V9=1
```

**Linux/macOS:**
```bash
export MAILSPRING_DEV_RECREATE_V9=1
```

### 2. 启动应用

正常启动应用，系统会自动：
1. 删除V9版本创建的所有表（Summary、SummaryTag、SummaryTagRelation、ContactRelation、MessageFullWithFileView）
2. 重新执行V9_SETUP_QUERIES中的所有建表语句
3. 在控制台输出执行过程

### 3. 清除环境变量

使用完毕后，记得清除环境变量：

**Windows (PowerShell):**
```powershell
Remove-Item Env:MAILSPRING_DEV_RECREATE_V9
```

**Windows (CMD):**
```cmd
set MAILSPRING_DEV_RECREATE_V9=
```

**Linux/macOS:**
```bash
unset MAILSPRING_DEV_RECREATE_V9
```

## 注意事项

1. 此功能会删除V9表及其数据，请确保在开发环境中使用
2. 重新创建表后，原有的V9表数据会丢失
3. 此功能不影响其他版本的表和数据
4. 建议在修改V9_SETUP_QUERIES后立即使用此功能测试

## 相关文件

- `constants.h`: 包含V9_SETUP_QUERIES和V9_DROP_QUERIES定义
- `MailStore.cpp`: 包含重新创建逻辑
- `MailStore.hpp`: 相关方法声明 