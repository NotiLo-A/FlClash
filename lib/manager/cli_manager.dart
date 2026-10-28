import 'dart:async';
import 'dart:convert';
import 'dart:io';

import 'package:fl_clash/common/common.dart';
import 'package:fl_clash/common/window.dart';
import 'package:fl_clash/enum/enum.dart';
import 'package:fl_clash/models/models.dart';
import 'package:fl_clash/providers/providers.dart';
import 'package:flutter/services.dart';
import 'package:flutter/widgets.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';

class CliManager extends ConsumerStatefulWidget {
  final Widget child;

  const CliManager({super.key, required this.child});

  @override
  ConsumerState<CliManager> createState() => _CliManagerState();
}

class _CliManagerState extends ConsumerState<CliManager> {
  static const _channel = MethodChannel('flclash/cli');

  @override
  void initState() {
    super.initState();
    if (Platform.isLinux) {
      _channel.setMethodCallHandler(_handleMethodCall);
    }
  }

  @override
  void dispose() {
    if (Platform.isLinux) {
      _channel.setMethodCallHandler(null);
    }
    super.dispose();
  }

  Future<dynamic> _handleMethodCall(MethodCall call) async {
    switch (call.method) {
      case 'start':
        await ref.read(setupActionProvider.notifier).setRunning(true);
        _showVisualFeedback(currentAppLocalizations.start, MessageLevel.info);
        return 'Proxy started';
      case 'stop':
        await ref.read(setupActionProvider.notifier).setRunning(false);
        _showVisualFeedback(currentAppLocalizations.stop, MessageLevel.info);
        return 'Proxy stopped';
      case 'toggle':
        final wasRunning = ref.read(isStartProvider);
        await ref.read(setupActionProvider.notifier).setRunning(!wasRunning);
        final isRunning = !wasRunning;
        _showVisualFeedback(
          isRunning
              ? currentAppLocalizations.start
              : currentAppLocalizations.stop,
          MessageLevel.info,
        );
        return isRunning ? 'Proxy started' : 'Proxy stopped';
      case 'restart-core':
        await ref.read(coreActionProvider.notifier).restartCore();
        _showVisualFeedback('Core restarted', MessageLevel.info);
        return 'Core restarted';
      case 'status':
        return _formatStatus();
      case 'status-json':
        return _formatStatusJson();
      case 'show':
        await window?.show();
        return 'Window raised';
      case 'hide':
        await window?.hide();
        return 'Window hidden';
      case 'quit':
        unawaited(_quit());
        return 'Exiting FlClash';
      default:
        throw PlatformException(
          code: 'unimplemented',
          message: 'Unknown CLI method: ${call.method}',
        );
    }
  }

  void _showVisualFeedback(String message, MessageLevel level) {
    if (ref.read(appVisibleProvider)) {
      dialogs.showNotifier(message, level: level);
    }
  }

  String _formatStatus() {
    final isRunning = ref.read(isStartProvider);
    final coreStatus = ref.read(coreStatusProvider);
    final mode = ref.read(patchClashConfigProvider).mode;
    final profile = ref.read(currentProfileProvider);
    final traffic =
        ref.read(trafficsProvider).list.lastOrNull ?? const Traffic();
    final up = traffic.up.traffic.show;
    final down = traffic.down.traffic.show;

    final statusSymbol = isRunning ? '●' : '○';
    final statusText = isRunning ? 'Running' : 'Stopped';
    final modeText = mode.name.toUpperCase();

    final buffer = StringBuffer();
    buffer.writeln('$statusSymbol FlClash: $statusText');
    buffer.writeln('  Core:    ${coreStatus.name}');
    buffer.writeln('  Mode:    $modeText');
    if (profile != null) {
      buffer.writeln('  Profile: ${profile.label}');
    }
    if (isRunning) {
      buffer.writeln('  Traffic: ↓ $down/s | ↑ $up/s');
    }
    return buffer.toString().trimRight();
  }

  String _formatStatusJson() {
    final isRunning = ref.read(isStartProvider);
    final coreStatus = ref.read(coreStatusProvider);
    final mode = ref.read(patchClashConfigProvider).mode;
    final profile = ref.read(currentProfileProvider);
    final traffic =
        ref.read(trafficsProvider).list.lastOrNull ?? const Traffic();

    return jsonEncode({
      'running': isRunning,
      'core': coreStatus.name,
      'mode': mode.name,
      'profile': profile?.label,
      'profileId': profile?.id,
      'traffic': {
        'up': traffic.up,
        'down': traffic.down,
      },
    });
  }

  Future<void> _quit() async {
    await system.exit();
    window?.forceExit();
  }

  @override
  Widget build(BuildContext context) {
    return widget.child;
  }
}
