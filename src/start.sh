#!/bin/sh

LIBPATH=/usr/local/lib
if [ x"$LIBPATH" != x ]; then
  if [ x"$LD_LIBRARY_PATH" = x ]; then
    LD_LIBRARY_PATH=$LIBPATH
  else
    LD_LIBRARY_PATH=$LIBPATH:$LD_LIBRARY_PATH
  fi
  export LD_LIBRARY_PATH
fi

DIR=`dirname $0`

player="${DIR}/warthog_player"
coach="${DIR}/warthog_coach"
teamname="Warthog2D"
host="localhost"
port=6000
coach_port=""
debug_server_host=""
debug_server_port=""

config="${DIR}/player.conf"
config_dir="${DIR}/formations-gear-dt"

coach_config="${DIR}/coach.conf"

number=11
usecoach="true"
savior="on"

sleepprog=sleep
goaliesleep=1
sleeptime=0.5

debugopt=""
coachdebug=""

usage()
{
  (echo "Usage: $0 [options]"
   echo "Possible options are:"
   echo "      --help                print this"
   echo "  -h, --host HOST           specifies server host"
   echo "  -p, --port PORT           specifies server port"
   echo "  -P  --coach-port PORT     specifies server port for online coach"
   echo "  -t, --teamname TEAMNAME   specifies team name"
   echo "  -n, --number NUMBER       specifies the number of players"
   echo "  -c, --with-coach          specifies to run the coach"
   echo "  -C, --without-coach       specifies not to run the coach"
   echo "  --savior (on|off)         enable/disable savior"
   echo "  --debug                   write debug log"
   echo "  --debug-coach             write debug log (coach)"
   echo "  --debug-connect           connect to the debug server"
   echo "  --debug-server-host HOST  specifies debug server host"
   echo "  --debug-server-port PORT  specifies debug server port"
   echo "  --debug-write             write debug server log"
   echo "  --log-dir DIRECTORY       specifies debug log directory"
   echo "  --log-ext EXTENSION       specifies debug log file extension") 1>&2
}

while [ $# -gt 0 ]
do
  case $1 in

    --help)
      usage
      exit 0
      ;;

    -h|--host)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      host=$2
      shift 1
      ;;

    -p|--port)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      port=$2
      shift 1
      ;;

    -P|--coach-port)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      coach_port=$2
      shift 1
      ;;

    -t|--teamname)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      teamname=$2
      shift 1
      ;;

    -n|--number)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      number=$2
      shift 1
      ;;

    -c|--with-coach)
      usecoach="true"
      ;;

    -C|--without-coach)
      usecoach="false"
      ;;

    --savior)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      savior=$2
      shift 1
      ;;

    --debug)
      debugopt="${debugopt} --debug"
      ;;

    --debug-coach)
      coachdebug="${coachdebug} --debug"
      ;;

    --debug-connect)
      debugopt="${debugopt} --debug_connect"
      ;;

    --debug-server-host)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debug_server_host=$2
      shift 1
      ;;

    --debug-server-port)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debug_server_port=$2
      shift 1
      ;;

    --debug-write)
      debugopt="${debugopt} --debug_write"
      ;;

    --log-dir)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debugopt="${debugopt} --log_dir ${2}"
      shift 1
      ;;

    --log-ext)
      if [ $# -lt 2 ]; then
        usage
        exit 1
      fi
      debugopt="${debugopt} --log_ext ${2}"
      shift 1
      ;;

    *)
      usage
      exit 1
      ;;
  esac

  shift 1
done

if [ $savior = "on" ] ; then
  savior="--savior on "
elif [ $savior = "off" ] ; then
  savior="--savior off "
else
  usage
  exit 1
fi

if [ X"${coach_port}" = X'' ]; then
  coach_port=`expr ${port} + 2`
fi

if [ X"${debug_server_host}" = X'' ]; then
  debug_server_host="${host}"
fi

if [ X"${debug_server_port}" = X'' ]; then
  debug_server_port=`expr ${port} + 32`
fi

opt="--player-config ${config} --config_dir ${config_dir}"
opt="${opt} -h ${host} -p ${port} -t ${teamname}"
opt="${opt} ${savior}"
opt="${opt} --debug_server_host ${debug_server_host}"
opt="${opt} --debug_server_port ${debug_server_port}"
opt="${opt} ${debugopt}"

if [ $number -gt 0 ]; then
  $player ${opt} -g &
  $sleepprog $goaliesleep
fi

i=2
while [ $i -le ${number} ] ; do
  $player ${opt} &
  $sleepprog $sleeptime
  i=`expr $i + 1`
done

if [ "${usecoach}" = "true" ]; then
  coachopt="--coach-config ${coach_config}"
  coachopt="${coachopt} -h ${host} -p ${coach_port} -t ${teamname}"
  coachopt="${coachopt} ${coachdebug}"
  $coach ${coachopt} &
fi
