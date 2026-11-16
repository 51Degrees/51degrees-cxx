# Each test runs the script on the entries of options.json for one shape of
# change and checks which entries it chooses, with the products each builds
# and its depth. The entries are read as common-ci's steps/select-options.ps1
# reads them, and common-ci builds nothing for a change to documentation
# alone, so no test gives the script that.
BeforeAll {
    $script:Script = Join-Path $PSScriptRoot 'select-options.ps1'
    $script:OptionsPath = Join-Path $PSScriptRoot 'options.json'

    # Returns each chosen entry's name with "<products> <depth>", and fails
    # on an entry that would measure performance.
    function Select-For([string[]]$Changed, [string[]]$CommentOnly = @()) {
        $options = @(Get-Content $script:OptionsPath -Raw |
            ConvertFrom-Json -AsHashtable)
        $chosen = & $script:Script -Options $options -ChangedFiles $Changed `
            -CommentOnlyFiles $CommentOnly 6>$null
        $result = @{}
        foreach ($entry in $chosen) {
            $entry.RunPerformance | Should -BeFalse -Because $entry.Name
            $result[$entry.Name] = "$($entry.Products -join ',') $($entry.Depth)"
        }
        return $result
    }

    function Assert-Chosen([hashtable]$Actual, [hashtable]$Expected) {
        @($Actual.Keys | Sort-Object) | Should -Be @($Expected.Keys | Sort-Object)
        foreach ($name in $Expected.Keys) {
            $Actual[$name] | Should -Be $Expected[$name] -Because $name
        }
    }
}

Describe 'select-options' {
    It 'builds Device Detection source on every entry that builds it' {
        Assert-Chosen (Select-For @('src/dd/hash/hash.c')) @{
            'Windows_VS_x64_Release' = 'dd standard'
            'Ubuntu_x64_Release' = 'dd standard'
            'Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly' = 'dd standard'
            'Windows_VS_x86_Release' = 'dd deep'
            'Windows_x64_Release' = 'dd deep'
            'Windows_x86_Release' = 'dd deep'
            'Ubuntu_x64_Debug_Asan' = 'dd deep'
            'Ubuntu_x86_Release' = 'dd deep'
        }
    }

    It 'builds a Device Detection test change on the standard set only' {
        Assert-Chosen (Select-For @('test/dd/hash/HashCTests.cpp')) @{
            'Windows_VS_x64_Release' = 'dd standard'
            'Ubuntu_x64_Release' = 'dd standard'
            'Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly' = 'dd standard'
        }
    }

    It 'runs the IP Intelligence tests when its source changes' {
        Assert-Chosen (Select-For @('src/ipi/ipi.c')) @{
            'Windows_VS_IPI_x64_Release_BuildOnly' = 'ipi deep'
            'Ubuntu_x64_Release' = 'ipi deep'
            'Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly' = 'ipi deep'
            'Windows_x64_Debug' = 'ipi deep'
            'Windows_x64_Release' = 'ipi deep'
            'Ubuntu_x64_Debug_Asan' = 'ipi deep'
            'Ubuntu_arm64_Release' = 'ipi deep'
        }
    }

    It 'leaves out the IP Intelligence tests for a change to its comments' {
        Assert-Chosen (Select-For @('src/ipi/ipi.c') @('src/ipi/ipi.c')) @{
            'Windows_VS_IPI_x64_Release_BuildOnly' = 'ipi standard'
            'Ubuntu_x64_Release' = 'ipi standard'
            'Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly' = 'ipi standard'
        }
    }

    It 'builds a change to common on every entry but the full build ones' {
        Assert-Chosen (Select-For @('src/common/file.c')) @{
            'Windows_VS_x64_Release' = 'common,dd standard'
            'Windows_VS_IPI_x64_Release_BuildOnly' = 'ipi deep'
            'Ubuntu_x64_Release' = 'common,dd,ipi deep'
            'Ubuntu_x64_Release_ExceptionsDisabled_BuildOnly' = 'dd,ipi deep'
            'Windows_VS_x64_Debug' = 'common deep'
            'Windows_VS_x86_Debug' = 'common deep'
            'Windows_VS_x86_Release' = 'common,dd deep'
            'Windows_x64_Debug' = 'common,ipi deep'
            'Windows_x64_Release' = 'common,dd,ipi deep'
            'Windows_x86_Debug' = 'common deep'
            'Windows_x86_Release' = 'common,dd deep'
            'Ubuntu_x64_Debug_Asan' = 'common,dd,ipi deep'
            'Ubuntu_x86_Release' = 'common,dd deep'
            'Ubuntu_x64_Release_CxxBothProducts' = 'dd,ipi deep'
            'Ubuntu_arm64_Release' = 'ipi deep'
        }
    }
}
