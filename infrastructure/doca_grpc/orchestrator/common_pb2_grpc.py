# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

import common_pb2 as common__pb2


class DocaOrchestrationStub(object):
    """DPU (Arm - gRPC Service) -> DPU (Arm - gRPC Program):
    =====================================================
    DOCA gRPC management API to allow the service to orchestrate
    the gRPC-supported program.
    """

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.HealthCheck = channel.unary_unary(
                '/DocaOrchestration/HealthCheck',
                request_serializer=common__pb2.HealthCheckReq.SerializeToString,
                response_deserializer=common__pb2.HealthCheckResp.FromString,
                )
        self.Destroy = channel.unary_unary(
                '/DocaOrchestration/Destroy',
                request_serializer=common__pb2.DestroyReq.SerializeToString,
                response_deserializer=common__pb2.DestroyResp.FromString,
                )


class DocaOrchestrationServicer(object):
    """DPU (Arm - gRPC Service) -> DPU (Arm - gRPC Program):
    =====================================================
    DOCA gRPC management API to allow the service to orchestrate
    the gRPC-supported program.
    """

    def HealthCheck(self, request, context):
        """Perform a Health Check (Ping) to a given gRPC-Supported program 
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def Destroy(self, request, context):
        """Destroy a given gRPC-Supported program 
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_DocaOrchestrationServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'HealthCheck': grpc.unary_unary_rpc_method_handler(
                    servicer.HealthCheck,
                    request_deserializer=common__pb2.HealthCheckReq.FromString,
                    response_serializer=common__pb2.HealthCheckResp.SerializeToString,
            ),
            'Destroy': grpc.unary_unary_rpc_method_handler(
                    servicer.Destroy,
                    request_deserializer=common__pb2.DestroyReq.FromString,
                    response_serializer=common__pb2.DestroyResp.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'DocaOrchestration', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class DocaOrchestration(object):
    """DPU (Arm - gRPC Service) -> DPU (Arm - gRPC Program):
    =====================================================
    DOCA gRPC management API to allow the service to orchestrate
    the gRPC-supported program.
    """

    @staticmethod
    def HealthCheck(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/DocaOrchestration/HealthCheck',
            common__pb2.HealthCheckReq.SerializeToString,
            common__pb2.HealthCheckResp.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def Destroy(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/DocaOrchestration/Destroy',
            common__pb2.DestroyReq.SerializeToString,
            common__pb2.DestroyResp.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)
