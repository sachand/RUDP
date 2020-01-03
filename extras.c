#include "extras.h"

char *
errno_to_string(int err)
{
	switch(err)
	{
	case EPERM:				return "EPERM";
	case ENOENT:			return "ENOENT";
	case ESRCH:				return "ESRCH";
	case EINTR:				return "EINTR";
	case EIO:				return "EIO";
	case ENXIO:				return "ENXIO";
	case E2BIG:				return "E2BIG";
	case ENOEXEC:			return "ENOEXEC";
	case EBADF:				return "EBADF";
	case ECHILD:			return "ECHILD";
	case EAGAIN:			return "EAGAIN";
	case ENOMEM:			return "ENOMEM";
	case EACCES:			return "EACCES";
	case EFAULT:			return "EFAULT";
	case ENOTBLK:			return "ENOTBLK";
	case EBUSY:				return "EBUSY";
	case EEXIST:			return "EEXIST";
	case EXDEV:				return "EXDEV";
	case ENODEV:			return "ENODEV";
	case ENOTDIR:			return "ENOTDIR";
	case EISDIR:			return "EISDIR";
	case EINVAL:			return "EINVAL";
	case ENFILE:			return "ENFILE";
	case EMFILE:			return "EMFILE";
	case ENOTTY:			return "ENOTTY";
	case ETXTBSY:			return "ETXTBSY";
	case EFBIG:				return "EFBIG";
	case ENOSPC:			return "ENOSPC";
	case ESPIPE:			return "ESPIPE";
	case EROFS:				return "EROFS";
	case EMLINK:			return "EMLINK";
	case EPIPE:				return "EPIPE";
	case EDOM:				return "EDOM";
	case ERANGE:			return "ERANGE";
	case ENAMETOOLONG:		return "ENAMETOOLONG";
	case ENOLCK:			return "ENOLCK";
	case ENOSYS:			return "ENOSYS";
	case ENOTEMPTY:			return "ENOTEMPTY";
	case ELOOP:				return "ELOOP";
	case ENOMSG:			return "ENOMSG";
	case EIDRM:				return "EIDRM";
	case ECHRNG:			return "ECHRNG";
	case EL2NSYNC:			return "EL2NSYNC";
	case EL3HLT:			return "EL3HLT";
	case EL3RST:			return "EL3RST";
	case ELNRNG:			return "ELNRNG";
	case EUNATCH:			return "EUNATCH";
	case ENOCSI:			return "ENOCSI";
	case EL2HLT:			return "EL2HLT";
	case EBADE:				return "EBADE";
	case EBADR:				return "EBADR";
	case EXFULL:			return "EXFULL";
	case ENOANO:			return "ENOANO";
	case EBADRQC:			return "EBADRQC";
	case EBADSLT:			return "EBADSLT";
	case EDEADLOCK:			return "EDEADLOCK";
	case EBFONT:			return "EBFONT";
	case ENOSTR:			return "ENOSTR";
	case ENODATA:			return "ENODATA";
	case ETIME:				return "ETIME";
	case ENOSR:				return "ENOSR";
	case ENONET:			return "ENONET";
	case ENOPKG:			return "ENOPKG";
	case EREMOTE:			return "EREMOTE";
	case ENOLINK:			return "ENOLINK";
	case EADV:				return "EADV";
	case ESRMNT:			return "ESRMNT";
	case ECOMM:				return "ECOMM";
	case EPROTO:			return "EPROTO";
	case EMULTIHOP:			return "EMULTIHOP";
	case EBADMSG:			return "EBADMSG";
	case EOVERFLOW:			return "EOVERFLOW";
	case ENOTUNIQ:			return "ENOTUNIQ";
	case EBADFD:			return "EBADFD";
	case EREMCHG:			return "EREMCHG";
	case ELIBACC:			return "ELIBACC";
	case ELIBBAD:			return "ELIBBAD";
	case ELIBSCN:			return "ELIBSCN";
	case ELIBMAX:			return "ELIBMAX";
	case ELIBEXEC:			return "ELIBEXEC";
	case EILSEQ:			return "EILSEQ";
	case ERESTART:			return "ERESTART";
	case ESTRPIPE:			return "ESTRPIPE";
	case EUSERS:			return "EUSERS";
	case ENOTSOCK:			return "ENOTSOCK";
	case EDESTADDRREQ:		return "EDESTADDRREQ";
	case EMSGSIZE:			return "EMSGSIZE";
	case EPROTOTYPE:		return "EPROTOTYPE";
	case ENOPROTOOPT:		return "ENOPROTOOPT";
	case EPROTONOSUPPORT:	return "EPROTONOSUPPORT";
	case ESOCKTNOSUPPORT:	return "ESOCKTNOSUPPORT";
	case EOPNOTSUPP:		return "EOPNOTSUPP";
	case EPFNOSUPPORT:		return "EPFNOSUPPORT";
	case EAFNOSUPPORT:		return "EAFNOSUPPORT";
	case EADDRINUSE:		return "EADDRINUSE";
	case EADDRNOTAVAIL:		return "EADDRNOTAVAIL";
	case ENETDOWN:			return "ENETDOWN";
	case ENETUNREACH:		return "ENETUNREACH";
	case ENETRESET:			return "ENETRESET";
	case ECONNABORTED:		return "ECONNABORTED";
	case ECONNRESET:		return "ECONNRESET";
	case ENOBUFS:			return "ENOBUFS";
	case EISCONN:			return "EISCONN";
	case ENOTCONN:			return "ENOTCONN";
	case ESHUTDOWN:			return "ESHUTDOWN";
	case ETOOMANYREFS:		return "ETOOMANYREFS";
	case ETIMEDOUT:			return "ETIMEDOUT";
	case ECONNREFUSED:		return "ECONNREFUSED";
	case EHOSTDOWN:			return "EHOSTDOWN";
	case EHOSTUNREACH:		return "EHOSTUNREACH";
	case EALREADY:			return "EALREADY";
	case EINPROGRESS:		return "EINPROGRESS";
	case ESTALE:			return "ESTALE";
	case EOWNERDEAD:		return "EOWNERDEAD";
	case ENOTRECOVERABLE:	return "ENOTRECOVERABLE";
	default:				return "ENONE";
	}
}

char *
errno_string()
{
	return errno_to_string(errno);
}

inline struct timeval
ms_to_timeval(uint32_t ms)
{
	struct timeval t;
	t.tv_sec = ms / 1000;
	t.tv_usec = (ms % 1000) * 1000;
	return t;
}

char *
signum_to_string(int sig)
{
    switch(sig)
    {
    case SIGINT :       return "SIGINT";
    case SIGHUP :       return "SIGHUP";
    case SIGTERM :      return "SIGTERM";
    case SIGPIPE :      return "SIGPIPE";
    case SIGCHLD :      return "SIGCHLD";
    default :           return "SIGUNKNOWN";
    }
}

boolean
is_flag_set (uint32_t holder, uint32_t mask)
{
    return 0 != (holder & mask) ? TRUE : FALSE;
}
