// Copyright (c) Microsoft Corporation
// The Microsoft Corporation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using Microsoft.CommandPalette.Extensions;
using Microsoft.CommandPalette.Extensions.Toolkit;

namespace LED-SSD-Extension;

public partial class LED-SSD-ExtensionCommandsProvider : CommandProvider
{
    private readonly ICommandItem[] _commands;

    public LED-SSD-ExtensionCommandsProvider()
    {
        DisplayName = "LED-SSD";
        Icon = IconHelpers.FromRelativePath("Assets\\StoreLogo.png");
        _commands = [
            new CommandItem(new LED-SSD-ExtensionPage()) { Title = DisplayName },
        ];
    }

    public override ICommandItem[] TopLevelCommands()
    {
        return _commands;
    }

}
