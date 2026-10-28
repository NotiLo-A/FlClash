class CliOptions {
  final bool isMinimized;
  final bool isStart;

  const CliOptions({
    this.isMinimized = false,
    this.isStart = false,
  });

  factory CliOptions.fromArgs(List<String> args) {
    var minimized = false;
    var start = false;
    for (final arg in args) {
      if (arg == '--minimized' || arg == '-m' || arg == '--hidden') {
        minimized = true;
      } else if (arg == '--start') {
        start = true;
      }
    }
    return CliOptions(
      isMinimized: minimized,
      isStart: start,
    );
  }
}

class CliStateHolder {
  CliOptions options = const CliOptions();
}

final cli = CliStateHolder();
