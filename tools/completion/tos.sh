#!/usr/bin/env bash

_tos_get_commands() {
    echo "version check new build flash monitor clean fullclean menuconfig savedef config_choice new_platform update help dev"
}

_tos_get_templates() {
    echo "base auduino"
}

_tos_find_config_files() {
    local config_dir="${1:-./config}"
    [ -d "$config_dir" ] && find "$config_dir" -maxdepth 1 -name "*.config" -exec basename {} \; 2>/dev/null
}

_tos_get_dev_commands() {
    echo "spc ceb bac"
}

_tos_get_flash_monitor_options() {
    echo "-d -b"
}

if [ -n "$BASH_VERSION" ] && [ -z "$POSIXLY_CORRECT" ]; then
    # Bash completion
    if type complete >/dev/null 2>&1; then
        _tos_completions() {
            local cur="${COMP_WORDS[COMP_CWORD]}"
            local prev="${COMP_WORDS[COMP_CWORD-1]}"
            COMPREPLY=()

            case ${prev} in
                version|check|build|clean|fullclean|menuconfig|savedef|update|help)
                    return 0
                    ;;
                new)
                    COMPREPLY=( $(compgen -W "$(_tos_get_templates)" -- ${cur}) )
                    return 0
                    ;;
                config_choice)
                    local configs=$(_tos_find_config_files)
                    [ -n "$configs" ] && COMPREPLY=( $(compgen -W "${configs}" -- ${cur}) )
                    return 0
                    ;;
                flash|monitor)
                    [ "${COMP_WORDS[COMP_CWORD-2]}" = "-d" ] && return 0
                    COMPREPLY=( $(compgen -W "$(_tos_get_flash_monitor_options)" -- ${cur}) )
                    return 0
                    ;;
                dev)
                    COMPREPLY=( $(compgen -W "$(_tos_get_dev_commands)" -- ${cur}) )
                    return 0
                    ;;
            esac

            [[ ${cur} == * ]] && COMPREPLY=( $(compgen -W "$(_tos_get_commands)" -- ${cur}) )
            return 0
        }

        complete -F _tos_completions tos
    fi

elif [ -n "$ZSH_VERSION" ]; then
    # Zsh completion
    { autoload -U compinit && compinit -u; } &>/dev/null
    
    if typeset -f compdef >/dev/null 2>&1; then
        _tos() {
            local state line
            typeset -A opt_args
            
            _arguments -C \
                '1: :->command' \
                '*: :->args'
            
            case $state in
                command)
                    local cmds=($(_tos_get_commands))
                    _describe -t commands 'command' cmds
                    ;;
                args)
                    case ${line[1]} in
                        new)
                            local templates=($(_tos_get_templates))
                            _describe -t templates 'templates' templates
                            ;;
                        config_choice)
                            local configs=($(_tos_find_config_files))
                            _describe -t configs 'config files' configs
                            ;;
                        flash|monitor)
                            if [[ ${line[-2]} != "-d" ]]; then
                                local options=($(_tos_get_flash_monitor_options))
                                _describe -t options 'options' options
                            fi
                            ;;
                        dev)
                            local dev_cmds=($(_tos_get_dev_commands))
                            _describe -t dev_commands 'dev commands' dev_cmds
                            ;;
                    esac
                    ;;
            esac
            return 0
        }
        
        compdef _tos tos 2>/dev/null
    fi
fi